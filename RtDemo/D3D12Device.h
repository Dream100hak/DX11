#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// D3D12Device — DX11 엔진의 Graphics 에 대응하는 DX12 디바이스/스왑체인 래퍼.
// Phase 0: 디바이스/큐/스왑체인/RTV힙/프레임 동기화/화면 클리어.
// Phase 1: 깊이버퍼 + 루트시그니처 + PSO + 정점/인덱스/상수 버퍼 → 조명 큐브 래스터.
// ───────────────────────────────────────────────────────────
class D3D12Device
{
public:
	static const UINT FRAME_COUNT = 2; // 더블 버퍼

	void Init(HWND hwnd, UINT width, UINT height);
	void Render();
	void Destroy();

	bool                  SupportsDXR() const { return _dxrTier >= D3D12_RAYTRACING_TIER_1_0; }
	D3D12_RAYTRACING_TIER GetDXRTier() const { return _dxrTier; }
	const std::wstring&   GetAdapterName() const { return _adapterName; }

private:
	// Phase 0
	void EnableDebugLayer();
	void CreateDeviceAndQueue();
	void CreateSwapChain(HWND hwnd);
	void CreateRtvHeapAndTargets();
	void CreateFrameResources();
	void WaitForGpu();
	void MoveToNextFrame();

	// Phase 1
	void CreateDepthBuffer();
	void CreateRootSignature();
	void CreatePipeline();
	void CreateCubeGeometry();
	void CreateConstantBuffers();
	ComPtr<ID3D12Resource> CreateUploadBuffer(const void* data, size_t size); // 단순 업로드힙 버퍼

	// Phase 2 (DXR)
	void BuildAccelerationStructures(); // 정적 지오메트리 → BLAS + TLAS (1회)
	ComPtr<ID3D12Resource> CreateDefaultBuffer(UINT64 size, D3D12_RESOURCE_STATES state, bool allowUAV);

private:
	UINT _width = 0;
	UINT _height = 0;

	ComPtr<IDXGIFactory6>              _factory;
	ComPtr<ID3D12Device5>             _device;
	ComPtr<ID3D12CommandQueue>        _queue;
	ComPtr<IDXGISwapChain3>           _swapChain;

	ComPtr<ID3D12DescriptorHeap>      _rtvHeap;
	UINT                              _rtvDescSize = 0;
	ComPtr<ID3D12Resource>            _renderTargets[FRAME_COUNT];

	ComPtr<ID3D12CommandAllocator>    _allocators[FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList4> _cmdList;

	ComPtr<ID3D12Fence>               _fence;
	UINT64                            _fenceValues[FRAME_COUNT] = {};
	HANDLE                            _fenceEvent = nullptr;
	UINT                              _frameIndex = 0;

	// Phase 1 — 깊이 / 파이프라인 / 지오메트리 / 상수
	ComPtr<ID3D12DescriptorHeap>      _dsvHeap;
	ComPtr<ID3D12Resource>            _depth;
	ComPtr<ID3D12RootSignature>       _rootSig;
	ComPtr<ID3D12PipelineState>       _pso;
	ComPtr<ID3D12Resource>            _vb;
	D3D12_VERTEX_BUFFER_VIEW          _vbv{};
	ComPtr<ID3D12Resource>            _ib;
	D3D12_INDEX_BUFFER_VIEW           _ibv{};
	UINT                              _indexCount = 0;
	ComPtr<ID3D12Resource>            _cb[FRAME_COUNT];
	void*                             _cbMapped[FRAME_COUNT] = {};

	// Phase 2 — 가속구조 (BLAS/TLAS), RayQuery 인라인 RT 용
	ComPtr<ID3D12Resource>            _blas;
	ComPtr<ID3D12Resource>            _blasScratch;
	ComPtr<ID3D12Resource>            _tlas;
	ComPtr<ID3D12Resource>            _tlasScratch;
	ComPtr<ID3D12Resource>            _instanceBuffer;
	UINT                              _vertexCount = 0;

	D3D12_RAYTRACING_TIER             _dxrTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	std::wstring                      _adapterName;
	float                             _time = 0.f;
};
