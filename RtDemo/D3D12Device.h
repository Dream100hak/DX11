#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// D3D12Device — DX11 엔진의 Graphics 에 대응하는 DX12 디바이스/스왑체인 래퍼.
// Phase 0: 디바이스 / 커맨드 큐 / 스왑체인(플립) / RTV 힙 / 프레임 동기화 / 화면 클리어.
// DXR 지원 티어를 질의해 보관 (이후 Phase 2 에서 가속구조/RT 파이프라인에 사용).
// ───────────────────────────────────────────────────────────
class D3D12Device
{
public:
	static const UINT FRAME_COUNT = 2; // 더블 버퍼

	void Init(HWND hwnd, UINT width, UINT height);
	void Render();          // Phase 0: 클리어 + Present
	void Destroy();

	bool                  SupportsDXR() const { return _dxrTier >= D3D12_RAYTRACING_TIER_1_0; }
	D3D12_RAYTRACING_TIER GetDXRTier() const { return _dxrTier; }
	const std::wstring&   GetAdapterName() const { return _adapterName; }

private:
	void EnableDebugLayer();
	void CreateDeviceAndQueue();
	void CreateSwapChain(HWND hwnd);
	void CreateRtvHeapAndTargets();
	void CreateFrameResources();

	void WaitForGpu();        // GPU 완전 대기 (리사이즈/종료용)
	void MoveToNextFrame();   // 프레임 펜스 진행 + 다음 백버퍼 대기

private:
	UINT _width = 0;
	UINT _height = 0;

	ComPtr<IDXGIFactory6>           _factory;
	ComPtr<ID3D12Device5>           _device;   // Device5 = DXR 인터페이스
	ComPtr<ID3D12CommandQueue>      _queue;
	ComPtr<IDXGISwapChain3>         _swapChain;

	ComPtr<ID3D12DescriptorHeap>    _rtvHeap;
	UINT                            _rtvDescSize = 0;
	ComPtr<ID3D12Resource>          _renderTargets[FRAME_COUNT];

	ComPtr<ID3D12CommandAllocator>  _allocators[FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList4> _cmdList; // CommandList4 = DXR DispatchRays 지원

	// GPU↔CPU 동기화 (프레임별 펜스 값)
	ComPtr<ID3D12Fence>             _fence;
	UINT64                          _fenceValues[FRAME_COUNT] = {};
	HANDLE                          _fenceEvent = nullptr;
	UINT                            _frameIndex = 0;

	D3D12_RAYTRACING_TIER           _dxrTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	std::wstring                    _adapterName;
	float                           _time = 0.f;
};
