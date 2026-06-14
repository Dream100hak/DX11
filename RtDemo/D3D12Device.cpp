#include "D3D12Device.h"

void D3D12Device::Init(HWND hwnd, UINT width, UINT height)
{
	_width = width;
	_height = height;

	EnableDebugLayer();
	CreateDeviceAndQueue();
	CreateSwapChain(hwnd);
	CreateRtvHeapAndTargets();
	CreateFrameResources();
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

	// 고성능 GPU 우선 선택 후 FEATURE_LEVEL_12_1 로 디바이스 생성
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

	// DXR 지원 티어 질의
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5))))
		_dxrTier = opt5.RaytracingTier;

	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
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
	ThrowIfFailed(_factory->CreateSwapChainForHwnd(_queue.Get(), hwnd, &scd, nullptr, nullptr, &sc1),
	              "CreateSwapChainForHwnd");
	_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	ThrowIfFailed(sc1.As(&_swapChain), "SwapChain As IDXGISwapChain3");
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::CreateRtvHeapAndTargets()
{
	D3D12_DESCRIPTOR_HEAP_DESC rd{};
	rd.NumDescriptors = FRAME_COUNT;
	rd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(_device->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&_rtvHeap)), "CreateDescriptorHeap RTV");
	_rtvDescSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargets[i])), "SwapChain GetBuffer");
		_device->CreateRenderTargetView(_renderTargets[i].Get(), nullptr, rtv);
		rtv.ptr += _rtvDescSize;
	}
}

void D3D12Device::CreateFrameResources()
{
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		ThrowIfFailed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_allocators[i])),
		              "CreateCommandAllocator");

	ThrowIfFailed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _allocators[_frameIndex].Get(),
	              nullptr, IID_PPV_ARGS(&_cmdList)), "CreateCommandList");
	ThrowIfFailed(_cmdList->Close(), "CommandList Close");

	ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)), "CreateFence");
	_fenceValues[_frameIndex] = 1;
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
}

void D3D12Device::Render()
{
	_time += 1.0f / 60.0f;

	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), nullptr), "CmdList Reset");

	// PRESENT → RENDER_TARGET 배리어
	D3D12_RESOURCE_BARRIER toRT{};
	toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toRT.Transition.pResource = _renderTargets[_frameIndex].Get();
	toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &toRT);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += SIZE_T(_frameIndex) * _rtvDescSize;
	_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

	// 시간에 따라 부드럽게 변하는 클리어 색 (동작 확인용)
	float r = 0.5f + 0.5f * sinf(_time * 0.7f);
	float g = 0.5f + 0.5f * sinf(_time * 0.9f + 2.0f);
	float b = 0.5f + 0.5f * sinf(_time * 1.3f + 4.0f);
	float clear[4] = { r * 0.4f, g * 0.4f, b * 0.5f, 1.0f };
	_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

	// RENDER_TARGET → PRESENT 배리어
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

	// 다음 백버퍼의 이전 작업이 끝났는지 확인
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
