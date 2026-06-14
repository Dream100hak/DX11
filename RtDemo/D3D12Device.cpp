#include "D3D12Device.h"
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

using namespace DirectX;

// ─── 상수버퍼 레이아웃 (HLSL SceneCB 와 일치, row_major) ───
struct SceneCB
{
	XMFLOAT4X4 mvp;
	XMFLOAT4X4 model;
	XMFLOAT4   lightDir;
	XMFLOAT4   camPos;
};

struct Vtx
{
	XMFLOAT3 pos;
	XMFLOAT3 nrm;
};

// 래스터 셰이더 (SM5.1, FXC). row_major 로 DirectXMath(행우선)와 일치 → 전치 불필요.
static const char* kMeshShader = R"(
cbuffer SceneCB : register(b0)
{
    row_major float4x4 gMVP;
    row_major float4x4 gModel;
    float4 gLightDir;
    float4 gCamPos;
};
RaytracingAccelerationStructure gScene : register(t0); // TLAS (인라인 RT)

struct VSIn  { float3 pos : POSITION; float3 nrm : NORMAL; };
struct VSOut { float4 pos : SV_POSITION; float3 wnrm : NORMAL; float3 wpos : TEXCOORD0; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos  = mul(float4(i.pos, 1.0), gMVP);
    o.wnrm = mul(i.nrm, (float3x3)gModel);
    o.wpos = mul(float4(i.pos, 1.0), gModel).xyz;
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET
{
    float3 N = normalize(i.wnrm);
    float3 L = normalize(-gLightDir.xyz);
    float  ndl = saturate(dot(N, L));

    // ── 하드웨어 레이트레이싱 그림자 (RayQuery, 인라인 RT) ──
    float shadow = 1.0;
    if (ndl > 0.0)
    {
        RayDesc ray;
        ray.Origin    = i.wpos + N * 0.02; // 셀프히트 바이어스
        ray.Direction = L;
        ray.TMin = 0.001;
        ray.TMax = 100.0;

        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> q;
        q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, ray);
        q.Proceed();
        if (q.CommittedStatus() != COMMITTED_NOTHING)
            shadow = 0.0; // 빛으로 가는 길이 막힘 → 그림자
    }

    float3 base = float3(0.80, 0.55, 0.38);
    float3 col = base * (0.18 + 0.82 * ndl * shadow); // 앰비언트 + 직접광(그림자 적용)
    return float4(col, 1.0);
}
)";

// ───────────────────────────────────────────────────────────
void D3D12Device::Init(HWND hwnd, UINT width, UINT height)
{
	_width = width;
	_height = height;

	EnableDebugLayer();
	CreateDeviceAndQueue();
	CreateSwapChain(hwnd);
	CreateRtvHeapAndTargets();
	CreateFrameResources();

	// Phase 1
	CreateDepthBuffer();
	CreateRootSignature();
	CreatePipeline();
	CreateCubeGeometry();
	CreateConstantBuffers();

	// Phase 2 — 정적 지오메트리 가속구조
	BuildAccelerationStructures();
}

void D3D12Device::EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		debug->EnableDebugLayer();
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
	// 루트 파라미터: 0 = CBV(b0) SceneCB, 1 = SRV(t0) TLAS (RayQuery)
	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // TLAS 를 루트 SRV 로 바인딩
	params[1].Descriptor.ShaderRegister = 0; // t0
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 2;
	rs.pParameters = params;
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

void D3D12Device::CreatePipeline()
{
	ComPtr<IDxcBlob> vs = CompileDxc(kMeshShader, L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ps = CompileDxc(kMeshShader, L"PSMain", L"ps_6_5");

	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso.SampleDesc.Count = 1;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "CreatePSO");
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

void D3D12Device::CreateCubeGeometry()
{
	// 정적 월드공간 지오메트리 (model=항등) — 바닥 위에 큐브.
	// RT 가 같은 버퍼로 BLAS 를 만들어 동일 지오메트리에 그림자 레이를 쏜다.
	std::vector<Vtx> verts;
	std::vector<uint16_t> indices;

	auto addQuad = [&](XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d, XMFLOAT3 n)
	{
		uint16_t base = (uint16_t)verts.size();
		verts.push_back({ a, n }); verts.push_back({ b, n });
		verts.push_back({ c, n }); verts.push_back({ d, n });
		indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
		indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 3);
	};

	// 큐브 (중심 y=0.9, 반치수 0.8 → y 0.1~1.7)
	const float h = 0.8f, cy = 0.9f;
	auto P = [&](float x, float y, float z) { return XMFLOAT3(x, y + cy, z); };
	addQuad(P(-h, h,-h), P(-h, h, h), P( h, h, h), P( h, h,-h), { 0, 1, 0}); // top
	addQuad(P(-h,-h, h), P(-h,-h,-h), P( h,-h,-h), P( h,-h, h), { 0,-1, 0}); // bottom
	addQuad(P( h, h,-h), P( h, h, h), P( h,-h, h), P( h,-h,-h), { 1, 0, 0}); // +X
	addQuad(P(-h, h, h), P(-h, h,-h), P(-h,-h,-h), P(-h,-h, h), {-1, 0, 0}); // -X
	addQuad(P( h, h, h), P(-h, h, h), P(-h,-h, h), P( h,-h, h), { 0, 0, 1}); // +Z
	addQuad(P(-h, h,-h), P( h, h,-h), P( h,-h,-h), P(-h,-h,-h), { 0, 0,-1}); // -Z

	// 바닥 평면 (y=0, ±6)
	const float g = 6.f;
	addQuad(XMFLOAT3(-g, 0, g), XMFLOAT3(g, 0, g), XMFLOAT3(g, 0,-g), XMFLOAT3(-g, 0,-g), { 0, 1, 0 });

	_vertexCount = (UINT)verts.size();
	_indexCount = (UINT)indices.size();

	const size_t vbSize = verts.size() * sizeof(Vtx);
	const size_t ibSize = indices.size() * sizeof(uint16_t);

	_vb = CreateUploadBuffer(verts.data(), vbSize);
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbv.StrideInBytes = sizeof(Vtx);
	_vbv.SizeInBytes = (UINT)vbSize;

	_ib = CreateUploadBuffer(indices.data(), ibSize);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibv.Format = DXGI_FORMAT_R16_UINT;
	_ibv.SizeInBytes = (UINT)ibSize;
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

void D3D12Device::BuildAccelerationStructures()
{
	// ── BLAS 입력 (삼각형 지오메트리) ──
	D3D12_RAYTRACING_GEOMETRY_DESC geom{};
	geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geom.Triangles.VertexBuffer.StartAddress = _vb->GetGPUVirtualAddress();
	geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vtx);
	geom.Triangles.VertexCount = _vertexCount;
	geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geom.Triangles.IndexBuffer = _ib->GetGPUVirtualAddress();
	geom.Triangles.IndexCount = _indexCount;
	geom.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasIn{};
	blasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	blasIn.NumDescs = 1;
	blasIn.pGeometryDescs = &geom;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo{};
	_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasIn, &blasInfo);

	_blas        = CreateDefaultBuffer(blasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_blasScratch = CreateDefaultBuffer(blasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// ── TLAS 인스턴스 (항등 변환, 1개) ──
	D3D12_RAYTRACING_INSTANCE_DESC inst{};
	inst.Transform[0][0] = 1.f; inst.Transform[1][1] = 1.f; inst.Transform[2][2] = 1.f;
	inst.InstanceMask = 0xFF;
	inst.AccelerationStructure = _blas->GetGPUVirtualAddress();
	_instanceBuffer = CreateUploadBuffer(&inst, sizeof(inst));

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasIn{};
	tlasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	tlasIn.NumDescs = 1;
	tlasIn.InstanceDescs = _instanceBuffer->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo{};
	_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasIn, &tlasInfo);

	_tlas        = CreateDefaultBuffer(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_tlasScratch = CreateDefaultBuffer(tlasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// ── 빌드 명령 기록/실행 (1회, 완료까지 대기) ──
	ThrowIfFailed(_allocators[_frameIndex]->Reset(), "AS alloc reset");
	ThrowIfFailed(_cmdList->Reset(_allocators[_frameIndex].Get(), nullptr), "AS cmd reset");

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuild{};
	blasBuild.Inputs = blasIn;
	blasBuild.DestAccelerationStructureData = _blas->GetGPUVirtualAddress();
	blasBuild.ScratchAccelerationStructureData = _blasScratch->GetGPUVirtualAddress();
	_cmdList->BuildRaytracingAccelerationStructure(&blasBuild, 0, nullptr);

	D3D12_RESOURCE_BARRIER uav{};
	uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav.UAV.pResource = _blas.Get();
	_cmdList->ResourceBarrier(1, &uav); // BLAS 완료 후 TLAS 가 읽도록

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
	tlasBuild.Inputs = tlasIn;
	tlasBuild.DestAccelerationStructureData = _tlas->GetGPUVirtualAddress();
	tlasBuild.ScratchAccelerationStructureData = _tlasScratch->GetGPUVirtualAddress();
	_cmdList->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);

	ThrowIfFailed(_cmdList->Close(), "AS cmd close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();
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

void D3D12Device::Render()
{
	_time += 1.0f / 60.0f;

	// ── 상수버퍼 갱신 (정적 지오메트리=항등, 빛 방향 애니메이션 → RT 그림자 이동) ──
	XMMATRIX model = XMMatrixIdentity();
	XMVECTOR eye = XMVectorSet(4.5f, 3.5f, -6.0f, 1.f);
	XMVECTOR at  = XMVectorSet(0.f, 0.7f, 0.f, 1.f);
	XMVECTOR up  = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
	float aspect = float(_width) / float(_height);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(55.f), aspect, 0.1f, 100.f);

	// 빛이 머리 위를 천천히 도는 형태 (그림자가 바닥을 쓸고 지나감)
	float a = _time * 0.6f;
	XMFLOAT4 lightDir(cosf(a) * 0.6f, -1.0f, sinf(a) * 0.6f, 0.f);

	SceneCB cb;
	XMStoreFloat4x4(&cb.mvp, model * view * proj); // row_major HLSL → 전치 불필요
	XMStoreFloat4x4(&cb.model, model);
	cb.lightDir = lightDir;
	XMStoreFloat4(&cb.camPos, eye);
	memcpy(_cbMapped[_frameIndex], &cb, sizeof(cb));

	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), _pso.Get()), "CmdList Reset");

	D3D12_RESOURCE_BARRIER toRT{};
	toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toRT.Transition.pResource = _renderTargets[_frameIndex].Get();
	toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &toRT);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += SIZE_T(_frameIndex) * _rtvDescSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	float clear[4] = { 0.06f, 0.07f, 0.10f, 1.0f };
	_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT vp{ 0.f, 0.f, float(_width), float(_height), 0.f, 1.f };
	D3D12_RECT sc{ 0, 0, LONG(_width), LONG(_height) };
	_cmdList->RSSetViewports(1, &vp);
	_cmdList->RSSetScissorRects(1, &sc);

	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
	_cmdList->SetGraphicsRootShaderResourceView(1, _tlas->GetGPUVirtualAddress()); // TLAS (RayQuery)
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, 1, &_vbv);
	_cmdList->IASetIndexBuffer(&_ibv);
	_cmdList->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);

	D3D12_RESOURCE_BARRIER toPresent = toRT;
	toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &toPresent);

	ThrowIfFailed(_cmdList->Close(), "CmdList Close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);

	ThrowIfFailed(_swapChain->Present(1, 0), "Present");
	MoveToNextFrame();
}

void D3D12Device::WaitForGpu()
{
	const UINT64 v = _fenceValues[_frameIndex];
	ThrowIfFailed(_queue->Signal(_fence.Get(), v), "Queue Signal");
	ThrowIfFailed(_fence->SetEventOnCompletion(v, _fenceEvent), "SetEventOnCompletion");
	WaitForSingleObject(_fenceEvent, INFINITE);
	_fenceValues[_frameIndex]++;
}

void D3D12Device::MoveToNextFrame()
{
	const UINT64 current = _fenceValues[_frameIndex];
	ThrowIfFailed(_queue->Signal(_fence.Get(), current), "Queue Signal");

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();

	if (_fence->GetCompletedValue() < _fenceValues[_frameIndex])
	{
		ThrowIfFailed(_fence->SetEventOnCompletion(_fenceValues[_frameIndex], _fenceEvent), "SetEventOnCompletion");
		WaitForSingleObject(_fenceEvent, INFINITE);
	}
	_fenceValues[_frameIndex] = current + 1;
}

void D3D12Device::Destroy()
{
	if (_device)
		WaitForGpu();
	if (_fenceEvent)
	{
		CloseHandle(_fenceEvent);
		_fenceEvent = nullptr;
	}
}
