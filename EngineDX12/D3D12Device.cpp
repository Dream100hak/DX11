#include "D3D12Device.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "GeometryHelper.h"
#include "SkyRenderer.h"
#include "GridRenderer.h"
#include "SceneManager.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#include <DirectXTex/DirectXTex.h>
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/DirectXTex_debug.lib")
#else
#pragma comment(lib, "DirectXTex/DirectXTex.lib")
#endif

D3D12Device* D3D12Device::s_main = nullptr;

using namespace DirectX;

// SceneCB 는 D3D12Device.h 로 이동 (Render.cpp 공용)
// 인라인 HLSL 셰이더 소스(kSceneCB/kMeshShader/...)는 D3D12Shaders.cpp 로 이동
#include "D3D12Shaders.h"

ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target); // 전방 선언


// ───────────────────────────────────────────────────────────
void D3D12Device::Init(HWND hwnd, UINT width, UINT height)
{
	s_main = this; // 전역 접근(Get) 등록
	_width = width;
	_height = height;
	_hwnd = hwnd;

	CoInitializeEx(nullptr, COINIT_MULTITHREADED); // WIC 텍스처 로딩용

	EnableDebugLayer();
	CreateDeviceAndQueue();
	CreateSwapChain(hwnd);
	CreateRtvHeapAndTargets();
	CreateFrameResources();

	// Phase 1
	CreateDepthBuffer();
	CreateRootSignature();
	CreatePipeline();
	CreateConstantBuffers();

	// 모델(기본 Archer) 로드 — 메시 + 바닥 + 텍스처 + BLAS/TLAS (런타임 교체 가능)
	_scene.Init(this);
	{
		wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
		_scene.Load(dir + L"\\..\\Resources\\Assets\\Models\\Archer\\Archer.mesh");
	}

	GET_SINGLE(TimeManager)->Init(); // 델타타임 기준점

	BuildGameScene(); // 씬 그래프(모델 GameObject + ModelRenderer) — 렌더 루프가 순회

	// Phase 3 — DDGI 프로브 볼륨
	CreateGI();

	// 포스트프로세스 (HDR 톤맵 / 블룸 / FXAA) — SceneRT SRV 생성 전에 힙/PSO 준비
	_postfx.Init(_device.Get(), _sceneFmt);
	_gamePostfx.Init(_device.Get(), _sceneFmt);                 // Game 뷰 전용 포스트
	_gameCB = CreateUploadBuffer(nullptr, sizeof(SceneCB));     // 게임 카메라 CB
	{ D3D12_RANGE nr{ 0,0 }; _gameCB->Map(0, &nr, &_gameCBMapped); }

	// 에디터 UI (ImGui DX12 + 도킹)
	InitEditor();
}

// 씬 그래프 구성 — 모델을 GameObject(Transform + ModelRenderer)로, SceneManager 현재 씬에 등록.
// 렌더 루프가 Scene 의 렌더러를 순회해 Draw(ctx) 한다 (DX11 Engine 의 Scene→Renderer 경로 이식 시작).
void D3D12Device::BuildGameScene()
{
	_gameScene = make_shared<Scene>();
	GET_SINGLE(SceneManager)->SetCurrentScene(_gameScene);

	_modelObj = make_shared<GameObject>();
	_modelObj->SetObjectName(L"Model");
	_modelObj->GetOrAddTransform();
	auto mr = make_shared<ModelRenderer>();
	mr->Bind(this);
	_modelObj->AddComponent(mr);
	_gameScene->Add(_modelObj);

	// 에디터 카메라 GameObject — Camera 컴포넌트 (GetMainCamera 캐시 대상). view/proj 는 FlyCamera 가 매 프레임 주입.
	_camObj = make_shared<GameObject>();
	_camObj->SetObjectName(L"EditorCamera");
	_camObj->SetEditorInternal(true);
	_camObj->GetOrAddTransform();
	_camObj->AddComponent(make_shared<Camera>());
	_gameScene->Add(_camObj);

	// 스카이박스(Background 큐) / 씬 그리드(Transparent 큐) GameObject
	{
		auto skyObj = make_shared<GameObject>();
		skyObj->SetObjectName(L"Sky"); skyObj->SetEditorInternal(true); skyObj->GetOrAddTransform();
		auto skyR = make_shared<SkyRenderer>(); skyR->Bind(this); skyObj->AddComponent(skyR);
		_gameScene->Add(skyObj);

		auto gridObj = make_shared<GameObject>();
		gridObj->SetObjectName(L"Grid"); gridObj->SetEditorInternal(true); gridObj->GetOrAddTransform();
		auto gridR = make_shared<GridRenderer>(); gridR->Bind(this); gridObj->AddComponent(gridR);
		_gameScene->Add(gridObj);
	}

	// 라이트 GameObject (씬 그래프 + CB 소스). 매 프레임 SyncLights 로 스칼라→컴포넌트 미러.
	auto addLight = [&](const wchar_t* name, LightType lt, Vec3 color, float intensity) -> shared_ptr<GameObject>
	{
		auto o = make_shared<GameObject>();
		o->SetObjectName(name); o->GetOrAddTransform();
		auto l = make_shared<Light>(); l->_lightType = lt; l->_color = color; l->_intensity = intensity;
		o->AddComponent(l);
		_gameScene->Add(o);
		return o;
	};
	_sunObj  = addLight(L"Directional Light", LightType::Directional, _sunColor, _lightIntensity);
	_ptObj   = addLight(L"Point Light",       LightType::Point,       _pointColor, _pointIntensity);
	_spotObj = addLight(L"Spot Light",        LightType::Spot,        _spotColor,  _spotIntensity);

	// 정적 메시 데모 — MeshRenderer 실드로우 검증용 큐브 (모델 옆, Opaque 버킷)
	{
		auto cubeObj = make_shared<GameObject>();
		cubeObj->SetObjectName(L"Cube");
		auto tr = cubeObj->GetOrAddTransform();
		tr->SetLocalPosition(Vec3{ 2.2f, 0.5f, 0.f });
		auto cmr = make_shared<MeshRenderer>(); cmr->Bind(this);
		vector<Vtx> cv; vector<uint32> ci; GeometryHelper::CreateCube(cv, ci, 1.0f, Vec3{ 1.f, 1.f, 1.f });
		cmr->SetGeometry(cv, ci); cmr->SetPrim(MeshPrim::Cube); // 복제/직렬화 복원 가능하도록 prim 지정
		// 절차적 체커 텍스처 (SRV 바인딩 경로 검증, 파일 의존 없음)
		{
			const uint32 N = 64; std::vector<uint8_t> tex(N * N * 4);
			for (uint32 y = 0; y < N; ++y) for (uint32 x = 0; x < N; ++x)
			{
				bool c = ((x / 8) + (y / 8)) & 1;
				uint8_t* p = &tex[(y * N + x) * 4];
				p[0] = c ? 230 : 60; p[1] = c ? 120 : 90; p[2] = c ? 60 : 200; p[3] = 255;
			}
			cmr->SetTexturePixels(tex, N, N);
		}
		cubeObj->AddComponent(cmr);
		_gameScene->Add(cubeObj);
	}
}

// 스칼라 라이팅 파라미터 → Light 컴포넌트 미러 (CB 가 컴포넌트에서 읽도록). 무중단 전환.
void D3D12Device::SyncLights()
{
	if (_sunObj) { auto l = _sunObj->GetLight(); l->_color = _sunColor; l->_intensity = _lightIntensity;
		float a = _lightAngle; l->_direction = Vec3{ cosf(a) * 0.6f, -1.f, sinf(a) * 0.6f }; }
	if (_ptObj)  { auto l = _ptObj->GetLight(); l->_enabled = _pointOn; l->_color = _pointColor; l->_intensity = _pointIntensity; l->_range = _pointRadius;
		if (auto t = _ptObj->GetTransform()) t->SetLocalPosition(_pointPos); }
	if (_spotObj){ auto l = _spotObj->GetLight(); l->_enabled = _spotOn; l->_color = _spotColor; l->_intensity = _spotIntensity; l->_range = _spotRadius;
		l->_spotAngleDeg = _spotConeDeg; l->_direction = _spotDir;
		if (auto t = _spotObj->GetTransform()) t->SetLocalPosition(_spotPos); }
}

void D3D12Device::EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		debug->EnableDebugLayer();
	// DRED — device removed 시 어떤 GPU 명령에서 터졌는지 breadcrumb/page-fault 기록
	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred))))
	{
		dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	}
#endif
}

// device removed 시 DRED breadcrumb/page-fault 덤프 (어느 GPU op 인지 추적)
void D3D12Device::DumpDeviceRemoved()
{
	HRESULT reason = _device ? _device->GetDeviceRemovedReason() : 0;
	char hdr[128]; sprintf_s(hdr, "\n===== DEVICE REMOVED  reason=0x%08X =====\n", (unsigned)reason);
	OutputDebugStringA(hdr); Log(hdr);
#if defined(_DEBUG)
	ComPtr<ID3D12DeviceRemovedExtendedData> dred;
	if (_device && SUCCEEDED(_device->QueryInterface(IID_PPV_ARGS(&dred))))
	{
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT bc{};
		if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&bc)))
		{
			const D3D12_AUTO_BREADCRUMB_NODE* n = bc.pHeadAutoBreadcrumbNode;
			while (n)
			{
				UINT done = n->pLastBreadcrumbValue ? *n->pLastBreadcrumbValue : 0;
				if (done < n->BreadcrumbCount) // 미완료 노드 = 폴트 지점 근처
				{
					char b[256]; sprintf_s(b, "[DRED] %S / %S  op %u/%u 에서 중단\n",
						n->pCommandQueueDebugNameW ? n->pCommandQueueDebugNameW : L"?",
						n->pCommandListDebugNameW ? n->pCommandListDebugNameW : L"?", done, n->BreadcrumbCount);
					OutputDebugStringA(b); Log(b);
					for (UINT i = done; i < n->BreadcrumbCount && i < done + 4; ++i)
					{ char o[64]; sprintf_s(o, "   op[%u]=%d\n", i, (int)n->pCommandHistory[i]); OutputDebugStringA(o); }
				}
				n = n->pNext;
			}
		}
		D3D12_DRED_PAGE_FAULT_OUTPUT pf{};
		if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pf)))
		{ char b[128]; sprintf_s(b, "[DRED] PageFault VA=0x%llX\n", (unsigned long long)pf.PageFaultVA); OutputDebugStringA(b); Log(b); }
	}
#endif
}

void D3D12Device::CreateDeviceAndQueue()
{
	UINT factoryFlags = 0;
#if defined(_DEBUG)
	factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&_factory)), "CreateDXGIFactory2");

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0;
	     _factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
	     ++i)
	{
		DXGI_ADAPTER_DESC1 desc{};
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device))))
		{
			_adapterName = desc.Description;
			break;
		}
		adapter.Reset();
	}
	if (!_device)
		ThrowIfFailed(E_FAIL, "No D3D12 12_1 adapter found");

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5))))
		_dxrTier = opt5.RaytracingTier;

	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&_queue)), "CreateCommandQueue");
}

void D3D12Device::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 scd{};
	scd.BufferCount = FRAME_COUNT;
	scd.Width = _width;
	scd.Height = _height;
	scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> sc1;
	ThrowIfFailed(_factory->CreateSwapChainForHwnd(_queue.Get(), hwnd, &scd, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd");
	_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	ThrowIfFailed(sc1.As(&_swapChain), "SwapChain As 3");
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::CreateRtvHeapAndTargets()
{
	D3D12_DESCRIPTOR_HEAP_DESC rd{};
	rd.NumDescriptors = FRAME_COUNT;
	rd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&_rtvHeap)), "CreateDescriptorHeap RTV");
	_rtvDescSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargets[i])), "GetBuffer");
		_device->CreateRenderTargetView(_renderTargets[i].Get(), nullptr, rtv);
		rtv.ptr += _rtvDescSize;
	}
}

// 창 크기 변경 — GPU 유휴 후 백버퍼 해제 → ResizeBuffers → RTV 재생성.
// 씬/게임 RT 는 ImGui 영역 기준으로 매 프레임 별도 리사이즈되므로 백버퍼만 처리.
void D3D12Device::OnResize(UINT width, UINT height)
{
	if (!_swapChain || width == 0 || height == 0) return;        // 최소화/초기화 전 무시
	if (width == _width && height == _height) return;

	WaitForGpu(); // 모든 프레임 in-flight 명령 완료 대기

	for (UINT i = 0; i < FRAME_COUNT; ++i) _renderTargets[i].Reset(); // 기존 백버퍼 참조 해제(ResizeBuffers 전제)

	_width = width; _height = height;

	DXGI_SWAP_CHAIN_DESC1 d{}; _swapChain->GetDesc1(&d);
	ThrowIfFailed(_swapChain->ResizeBuffers(FRAME_COUNT, width, height, d.Format, d.Flags), "ResizeBuffers");
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();

	// 백버퍼 RTV 재생성 (기존 _rtvHeap 슬롯 재사용)
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargets[i])), "GetBuffer resize");
		_device->CreateRenderTargetView(_renderTargets[i].Get(), nullptr, rtv);
		rtv.ptr += _rtvDescSize;
	}

	// 프레임별 펜스값을 현재값으로 정렬 (MoveToNextFrame 일관성)
	for (UINT i = 0; i < FRAME_COUNT; ++i) _fenceValues[i] = _fenceValues[_frameIndex];
}

void D3D12Device::CreateFrameResources()
{
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		ThrowIfFailed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_allocators[i])), "CreateCommandAllocator");

	ThrowIfFailed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _allocators[_frameIndex].Get(), nullptr, IID_PPV_ARGS(&_cmdList)), "CreateCommandList");
	ThrowIfFailed(_cmdList->Close(), "CmdList Close");

	ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)), "CreateFence");
	_fenceValues[_frameIndex] = 1;
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
}

// ─── Phase 1 ───
void D3D12Device::CreateDepthBuffer()
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = _width;
	rd.Height = _height;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_D32_FLOAT;
	rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv{};
	cv.Format = DXGI_FORMAT_D32_FLOAT;
	cv.DepthStencil.Depth = 1.0f;

	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&_depth)), "Create Depth");

	D3D12_DESCRIPTOR_HEAP_DESC dd{};
	dd.NumDescriptors = 1;
	dd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&dd, IID_PPV_ARGS(&_dsvHeap)), "CreateDescriptorHeap DSV");

	_device->CreateDepthStencilView(_depth.Get(), nullptr, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Device::CreateRootSignature()
{
	// 디퓨즈 텍스처(t2) 디스크립터 테이블 범위
	D3D12_DESCRIPTOR_RANGE texRange{};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.NumDescriptors = 3; // t2 디퓨즈, t3 노멀, t4 스펙큘러
	texRange.BaseShaderRegister = 2; // t2
	texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 루트: 0=CBV(b0), 1=SRV(t0)TLAS, 2=SRV(t1)프로브, 3=테이블(t2 텍스처), 4=루트상수(b1 useTex), 5=SRV(t5)프로브 depth
	D3D12_ROOT_PARAMETER params[6]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // TLAS
	params[1].Descriptor.ShaderRegister = 0; // t0
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // 프로브
	params[2].Descriptor.ShaderRegister = 1; // t1
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 프로브 시각화 VS 에서도 읽음

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &texRange;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[4].Constants.ShaderRegister = 1; // b1
	params[4].Constants.Num32BitValues = 8; // mode + metallic/roughness/emissive + tint.rgb + pad (per-object) / 파티클 빌보드 기저
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 파티클 VS 도 b1(빌보드 기저) 읽음

	params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // 프로브 depth
	params[5].Descriptor.ShaderRegister = 5; // t5
	params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 정적 샘플러 s0 (선형 WRAP)
	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	samp.ShaderRegister = 0; // s0
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 6;
	rs.pParameters = params;
	rs.NumStaticSamplers = 1;
	rs.pStaticSamplers = &samp;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "SerializeRootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "CreateRootSig");
}

// DXC 컴파일 (SM6.x) — RayQuery(인라인 RT, SM6.5) 때문에 FXC 대신 DXC 사용.
// row_major 키워드로 DirectXMath(행우선) 일치는 셰이더 소스에서 명시.
ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target)
{
	static ComPtr<IDxcUtils> utils;
	static ComPtr<IDxcCompiler3> compiler;
	if (!utils) ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DxcCreateInstance Utils");
	if (!compiler) ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), "DxcCreateInstance Compiler");

	DxcBuffer srcBuf{};
	srcBuf.Ptr = src;
	srcBuf.Size = strlen(src);
	srcBuf.Encoding = DXC_CP_UTF8;

	std::vector<LPCWSTR> args = { L"-E", entry, L"-T", target, L"-HV", L"2021" };
#if defined(_DEBUG)
	args.push_back(L"-Zi");
	args.push_back(L"-Qembed_debug");
	args.push_back(L"-Od");
#endif

	ComPtr<IDxcResult> result;
	ThrowIfFailed(compiler->Compile(&srcBuf, args.data(), (UINT)args.size(), nullptr, IID_PPV_ARGS(&result)), "DXC Compile");

	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
		OutputDebugStringA(errors->GetStringPointer());

	HRESULT status = S_OK;
	result->GetStatus(&status);
	ThrowIfFailed(status, "DXC shader has errors");

	ComPtr<IDxcBlob> code;
	ThrowIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&code), nullptr), "DXC GetOutput");
	return code;
}

// DDS 큐브맵 로드 → TextureCube DEFAULT 리소스(수동 멀티 서브리소스 업로드) + SRV 힙(t2~t4).
bool D3D12Device::LoadSkyCubemap(const std::wstring& ddsPath)
{
	using namespace DirectX;
	TexMetadata meta{}; ScratchImage scratch;
	if (FAILED(LoadFromDDSFile(ddsPath.c_str(), DDS_FLAGS_NONE, &meta, scratch))) { Log("SkyCubemap load FAILED (DDS)"); return false; }
	if (meta.arraySize < 6) { Log("SkyCubemap: not a cubemap (arraySize<6)"); return false; }
	const UINT mips = (UINT)meta.mipLevels;

	D3D12_HEAP_PROPERTIES hpD{}; hpD.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC td{};
	td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	td.Width = (UINT)meta.width; td.Height = (UINT)meta.height;
	td.DepthOrArraySize = 6; td.MipLevels = (UINT16)mips;
	td.Format = meta.format; td.SampleDesc.Count = 1;
	_skyCube.Reset();
	if (FAILED(_device->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_skyCube)))) { Log("SkyCubemap: CreateResource FAILED"); return false; }

	const UINT numSub = 6 * mips;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fps(numSub);
	std::vector<UINT> rowCnt(numSub); std::vector<UINT64> rowSz(numSub);
	UINT64 total = 0;
	_device->GetCopyableFootprints(&td, 0, numSub, 0, fps.data(), rowCnt.data(), rowSz.data(), &total);

	// 업로드 버퍼
	D3D12_HEAP_PROPERTIES hpU{}; hpU.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
	bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> upload;
	if (FAILED(_device->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) return false;
	uint8_t* mapped = nullptr; D3D12_RANGE nr{ 0, 0 }; upload->Map(0, &nr, (void**)&mapped);
	for (UINT face = 0; face < 6; ++face)
		for (UINT mip = 0; mip < mips; ++mip)
		{
			const Image* img = scratch.GetImage(mip, face, 0); if (!img) continue;
			UINT sub = face * mips + mip; auto& fp = fps[sub];
			for (UINT y = 0; y < rowCnt[sub]; ++y)
				memcpy(mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, img->pixels + (size_t)y * img->rowPitch, (size_t)rowSz[sub]);
		}
	upload->Unmap(0, nullptr);

	// 임시 커맨드리스트로 복사 + 배리어
	ComPtr<ID3D12CommandAllocator> al; ComPtr<ID3D12GraphicsCommandList> cl;
	_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al.Get(), nullptr, IID_PPV_ARGS(&cl));
	for (UINT sub = 0; sub < numSub; ++sub)
	{
		D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = _skyCube.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = sub;
		D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fps[sub];
		cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}
	D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = _skyCube.Get();
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; cl->ResourceBarrier(1, &b);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() }; _queue->ExecuteCommandLists(1, lists);
	ComPtr<ID3D12Fence> fence; _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr); _queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); } CloseHandle(ev);

	// SRV (TextureCube) — 3 디스크립터(t2~t4) 모두 큐브로(셰이더는 t2만 사용)
	D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 3; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_skyCubeHeap.Reset();
	if (FAILED(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_skyCubeHeap)))) return false;
	UINT inc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_SHADER_RESOURCE_VIEW_DESC sd{}; sd.Format = meta.format; sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sd.TextureCube.MipLevels = mips;
	D3D12_CPU_DESCRIPTOR_HANDLE h = _skyCubeHeap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < 3; ++i) { _device->CreateShaderResourceView(_skyCube.Get(), &sd, h); h.ptr += inc; }
	Log("SkyCubemap loaded (cube SRV ready)");
	return true;
}

void D3D12Device::CreatePipeline()
{
	ComPtr<IDxcBlob> vs = CompileDxc(kMeshShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ps = CompileDxc(kMeshShader.c_str(), L"PSMain", L"ps_6_5");

	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rast{};
	rast.FillMode = D3D12_FILL_MODE_SOLID;
	rast.CullMode = D3D12_CULL_MODE_NONE; // Phase 1: 안팎 모두 보이게
	rast.FrontCounterClockwise = FALSE;
	rast.DepthClipEnable = TRUE;

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC ds{};
	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig.Get();
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.InputLayout = { layout, _countof(layout) };
	pso.RasterizerState = rast;
	pso.BlendState = blend;
	pso.DepthStencilState = ds;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = _sceneFmt;
	pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso.SampleDesc.Count = 1;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "CreatePSO");

	// ── 와이어프레임 PSO (모델 토글) ──
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wpso = pso;
	wpso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&wpso, IID_PPV_ARGS(&_wirePSO)), "CreateWirePSO");

	// ── 스카이박스 PSO (풀스크린 삼각형, 깊이/입력레이아웃 없음, b0 만 사용) ──
	ComPtr<IDxcBlob> svs = CompileDxc(kSkyShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> sps = CompileDxc(kSkyShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC spso{};
	spso.pRootSignature = _rootSig.Get();
	spso.VS = { svs->GetBufferPointer(), svs->GetBufferSize() };
	spso.PS = { sps->GetBufferPointer(), sps->GetBufferSize() };
	spso.RasterizerState = rast;
	spso.BlendState = blend;
	spso.DepthStencilState.DepthEnable = FALSE;   // 배경 — 깊이 테스트/쓰기 없음
	spso.SampleMask = UINT_MAX;
	spso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	spso.NumRenderTargets = 1;
	spso.RTVFormats[0] = _sceneFmt;
	spso.DSVFormat = DXGI_FORMAT_UNKNOWN;
	spso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&spso, IID_PPV_ARGS(&_skyPSO)), "CreateSkyPSO");

	// ── 그리드 PSO (깊이 테스트 LESS_EQUAL/쓰기 없음, 알파 블렌드, SV_Depth 출력) ──
	ComPtr<IDxcBlob> gvs = CompileDxc(kGridShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> gps = CompileDxc(kGridShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpso = spso;
	gpso.VS = { gvs->GetBufferPointer(), gvs->GetBufferSize() };
	gpso.PS = { gps->GetBufferPointer(), gps->GetBufferSize() };
	gpso.DepthStencilState.DepthEnable = TRUE;
	gpso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	gpso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	gpso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	gpso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	gpso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	gpso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gpso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&gpso, IID_PPV_ARGS(&_gridPSO)), "CreateGridPSO");

	// ── 파티클 빌보드 PSO (StructuredBuffer 인스턴싱, 가산 블렌드, 깊이 테스트/쓰기 없음) ──
	{
		ComPtr<IDxcBlob> pvs = CompileDxc(kParticleShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> pps = CompileDxc(kParticleShader.c_str(), L"PSMain", L"ps_6_5");
		// 검증된 그리드 PSO(gpso)를 베이스로 — 입력레이아웃 없음/TRIANGLE/깊이테스트/DSV 동일, 차이만 오버라이드
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pp = gpso;
		pp.VS = { pvs->GetBufferPointer(), pvs->GetBufferSize() };
		pp.PS = { pps->GetBufferPointer(), pps->GetBufferSize() };
		pp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pp.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;      // 가산 블렌드
		pp.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		pp.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&pp, IID_PPV_ARGS(&_particlePSO)), "CreateParticlePSO");
	}

	// ── 터레인 테셀레이션 PSO (VS/HS/DS/PS, PATCH 토폴로지) — 메인 opaque pso 베이스 ──
	{
		ComPtr<IDxcBlob> tvs = CompileDxc(kTessShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> ths = CompileDxc(kTessShader.c_str(), L"HSMain", L"hs_6_5");
		ComPtr<IDxcBlob> tds = CompileDxc(kTessShader.c_str(), L"DSMain", L"ds_6_5");
		ComPtr<IDxcBlob> tps = CompileDxc(kTessShader.c_str(), L"PSMain", L"ps_6_5");
		D3D12_INPUT_ELEMENT_DESC til[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC tp = pso; // 메인 opaque(깊이 쓰기/테스트, RTV/DSV) 베이스
		tp.VS = { tvs->GetBufferPointer(), tvs->GetBufferSize() };
		tp.HS = { ths->GetBufferPointer(), ths->GetBufferSize() };
		tp.DS = { tds->GetBufferPointer(), tds->GetBufferSize() };
		tp.PS = { tps->GetBufferPointer(), tps->GetBufferSize() };
		tp.InputLayout = { til, _countof(til) };
		tp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		tp.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; // 테셀레이션
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&tp, IID_PPV_ARGS(&_tessPSO)), "CreateTessPSO");
		tp.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // 와이어 변형(테셀 밀도 시각화)
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&tp, IID_PPV_ARGS(&_tessWirePSO)), "CreateTessWirePSO");
	}

	// ── 물 평면 PSO (절차 그리드, 알파 블렌드, 깊이 테스트/쓰기) — 그리드 PSO(gpso) 베이스 ──
	{
		ComPtr<IDxcBlob> wvs = CompileDxc(kWaterShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> wps = CompileDxc(kWaterShader.c_str(), L"PSMain", L"ps_6_5");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC wp = gpso; // 입력레이아웃 없음/TRIANGLE/알파블렌드 베이스
		wp.VS = { wvs->GetBufferPointer(), wvs->GetBufferSize() };
		wp.PS = { wps->GetBufferPointer(), wps->GetBufferSize() };
		wp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		wp.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 물 표면 깊이 기록(파티클/그리드가 가려지게)
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&wp, IID_PPV_ARGS(&_waterPSO)), "CreateWaterPSO");
	}

	// ── 아웃라인 PSO (앞면 컬링 = 뒷면 렌더, 깊이 LESS/쓰기, 입력레이아웃 = 메시) ──
	ComPtr<IDxcBlob> ovs = CompileDxc(kOutlineShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ops = CompileDxc(kOutlineShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opso{};
	opso.pRootSignature = _rootSig.Get();
	opso.VS = { ovs->GetBufferPointer(), ovs->GetBufferSize() };
	opso.PS = { ops->GetBufferPointer(), ops->GetBufferSize() };
	opso.InputLayout = { layout, _countof(layout) };
	opso.RasterizerState = rast; opso.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	opso.BlendState = blend;
	opso.DepthStencilState = ds; // LESS, write on
	opso.SampleMask = UINT_MAX;
	opso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opso.NumRenderTargets = 1; opso.RTVFormats[0] = _sceneFmt;
	opso.DSVFormat = DXGI_FORMAT_D32_FLOAT; opso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&opso, IID_PPV_ARGS(&_outlinePSO)), "CreateOutlinePSO");

	// ── 프로브 시각화 PSO (POINTLIST, 깊이 테스트) ──
	ComPtr<IDxcBlob> pvs = CompileDxc(kProbeViz.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> pps = CompileDxc(kProbeViz.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ppso{};
	ppso.pRootSignature = _rootSig.Get();
	ppso.VS = { pvs->GetBufferPointer(), pvs->GetBufferSize() };
	ppso.PS = { pps->GetBufferPointer(), pps->GetBufferSize() };
	ppso.RasterizerState = rast;
	ppso.BlendState = blend;
	ppso.DepthStencilState.DepthEnable = TRUE; ppso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ppso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ppso.SampleMask = UINT_MAX;
	ppso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	ppso.NumRenderTargets = 1; ppso.RTVFormats[0] = _sceneFmt;
	ppso.DSVFormat = DXGI_FORMAT_D32_FLOAT; ppso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&ppso, IID_PPV_ARGS(&_probePSO)), "CreateProbePSO");

	// ── 디버그 라인(본/AABB/콘/아이콘/파티클) — DebugDraw 클래스가 PSO 2종 생성 ──
	_debugDraw.Init(_device.Get(), _rootSig.Get(), _sceneFmt);
}

ComPtr<ID3D12Resource> D3D12Device::CreateUploadBuffer(const void* data, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "CreateUploadBuffer");

	if (data)
	{
		void* p = nullptr;
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(buf->Map(0, &noRead, &p), "Map upload");
		memcpy(p, data, size);
		buf->Unmap(0, nullptr);
	}
	return buf;
}


ComPtr<ID3D12Resource> D3D12Device::CreateDefaultBuffer(UINT64 size, D3D12_RESOURCE_STATES state, bool allowUAV)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, nullptr, IID_PPV_ARGS(&buf)), "CreateDefaultBuffer");
	return buf;
}

void D3D12Device::Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to)
{
	if (cur == to) return;
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.StateBefore = cur;
	b.Transition.StateAfter = to;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &b);
	cur = to;
}

void D3D12Device::CreateGI()
{
	// gather 컴퓨트 셰이더는 공용 SceneCB 레이아웃에 의존 → 여기서 컴파일 후 바이트코드만 Ddgi 에 전달
	ComPtr<IDxcBlob> cs = CompileDxc(kGatherShader.c_str(), L"CSMain", L"cs_6_5");
	_ddgi.Create(_device.Get(), cs->GetBufferPointer(), cs->GetBufferSize());
}

void D3D12Device::CreateConstantBuffers()
{
	const size_t cbSize = (sizeof(SceneCB) + 255) & ~size_t(255); // 256 정렬
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		_cb[i] = CreateUploadBuffer(nullptr, cbSize);
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(_cb[i]->Map(0, &noRead, &_cbMapped[i]), "Map CB"); // 영속 매핑
	}
}


// 전체 플러시 — GPU 완료까지 대기 (스키닝이 VB 를 매 프레임 CPU 갱신하므로
// CPU/GPU 가 VB 를 동시에 만지지 않도록 프레임마다 직렬화. 단순/안전, 데모용)
void D3D12Device::WaitForGpu()
{
	const UINT64 v = ++_flushValue;
	ThrowIfFailed(_queue->Signal(_fence.Get(), v), "Queue Signal");
	if (_fence->GetCompletedValue() < v)
	{
		ThrowIfFailed(_fence->SetEventOnCompletion(v, _fenceEvent), "SetEventOnCompletion");
		WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void D3D12Device::MoveToNextFrame()
{
	WaitForGpu();
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::Destroy()
{
	if (_device)
		WaitForGpu();
	if (_editorReady)
	{
		_imgui.Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		_editorReady = false;
	}
	if (_fenceEvent)
	{
		CloseHandle(_fenceEvent);
		_fenceEvent = nullptr;
	}
}
