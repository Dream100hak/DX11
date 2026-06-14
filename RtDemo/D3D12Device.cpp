#include "D3D12Device.h"

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
struct VSIn  { float3 pos : POSITION; float3 nrm : NORMAL; };
struct VSOut { float4 pos : SV_POSITION; float3 wnrm : NORMAL; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos  = mul(float4(i.pos, 1.0), gMVP);
    o.wnrm = mul(i.nrm, (float3x3)gModel);
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET
{
    float3 N = normalize(i.wnrm);
    float3 L = normalize(-gLightDir.xyz);
    float  ndl = saturate(dot(N, L));
    float3 base = float3(0.85, 0.55, 0.35);
    float3 col = base * (0.2 + 0.8 * ndl);
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
	// 루트 파라미터 0 = CBV(b0) (SceneCB)
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.Descriptor.ShaderRegister = 0;
	param.Descriptor.RegisterSpace = 0;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 1;
	rs.pParameters = &param;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "SerializeRootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "CreateRootSig");
}

static ComPtr<ID3DBlob> CompileFromSource(const char* src, const char* entry, const char* target)
{
	UINT flags = 0;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ComPtr<ID3DBlob> code, err;
	HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &code, &err);
	if (FAILED(hr))
	{
		if (err) OutputDebugStringA(reinterpret_cast<const char*>(err->GetBufferPointer()));
		ThrowIfFailed(hr, "D3DCompile");
	}
	return code;
}

void D3D12Device::CreatePipeline()
{
	ComPtr<ID3DBlob> vs = CompileFromSource(kMeshShader, "VSMain", "vs_5_1");
	ComPtr<ID3DBlob> ps = CompileFromSource(kMeshShader, "PSMain", "ps_5_1");

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
	// 면별 법선을 위해 24정점 (6면 × 4)
	const Vtx v[] =
	{
		// +Y (top)
		{{-1, 1,-1},{0,1,0}}, {{-1, 1, 1},{0,1,0}}, {{ 1, 1, 1},{0,1,0}}, {{ 1, 1,-1},{0,1,0}},
		// -Y (bottom)
		{{-1,-1, 1},{0,-1,0}}, {{-1,-1,-1},{0,-1,0}}, {{ 1,-1,-1},{0,-1,0}}, {{ 1,-1, 1},{0,-1,0}},
		// +X
		{{ 1, 1,-1},{1,0,0}}, {{ 1, 1, 1},{1,0,0}}, {{ 1,-1, 1},{1,0,0}}, {{ 1,-1,-1},{1,0,0}},
		// -X
		{{-1, 1, 1},{-1,0,0}}, {{-1, 1,-1},{-1,0,0}}, {{-1,-1,-1},{-1,0,0}}, {{-1,-1, 1},{-1,0,0}},
		// +Z
		{{ 1, 1, 1},{0,0,1}}, {{-1, 1, 1},{0,0,1}}, {{-1,-1, 1},{0,0,1}}, {{ 1,-1, 1},{0,0,1}},
		// -Z
		{{-1, 1,-1},{0,0,-1}}, {{ 1, 1,-1},{0,0,-1}}, {{ 1,-1,-1},{0,0,-1}}, {{-1,-1,-1},{0,0,-1}},
	};
	uint16_t idx[36];
	for (int f = 0; f < 6; ++f)
	{
		uint16_t b = uint16_t(f * 4);
		uint16_t* o = idx + f * 6;
		o[0] = b; o[1] = b + 1; o[2] = b + 2;
		o[3] = b; o[4] = b + 2; o[5] = b + 3;
	}

	_vb = CreateUploadBuffer(v, sizeof(v));
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbv.StrideInBytes = sizeof(Vtx);
	_vbv.SizeInBytes = sizeof(v);

	_ib = CreateUploadBuffer(idx, sizeof(idx));
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibv.Format = DXGI_FORMAT_R16_UINT;
	_ibv.SizeInBytes = sizeof(idx);
	_indexCount = 36;
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

	// ── 상수버퍼 갱신 (회전 모델 + 카메라, LH) ──
	XMMATRIX model = XMMatrixRotationY(_time * 0.8f) * XMMatrixRotationX(0.3f);
	XMVECTOR eye = XMVectorSet(0.f, 1.5f, -5.f, 1.f);
	XMVECTOR at  = XMVectorSet(0.f, 0.f, 0.f, 1.f);
	XMVECTOR up  = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
	float aspect = float(_width) / float(_height);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), aspect, 0.1f, 100.f);

	SceneCB cb;
	XMStoreFloat4x4(&cb.mvp, model * view * proj); // row_major HLSL → 전치 불필요
	XMStoreFloat4x4(&cb.model, model);
	cb.lightDir = XMFLOAT4(0.4f, -1.0f, 0.5f, 0.f);
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
