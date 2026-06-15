#pragma once
#include "Common.h"
#include "MeshLoader.h"

// 정점 (래스터 입력 + RT BLAS 소스 + GI gather 소스 공용)
struct Vtx
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 nrm;
	DirectX::XMFLOAT3 col;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT3 tan;
};

// 상수버퍼 (HLSL SceneCB 와 일치, row_major)
struct SceneCB
{
	DirectX::XMFLOAT4X4 mvp;
	DirectX::XMFLOAT4X4 model;
	DirectX::XMFLOAT4   lightDir;  // xyz=방향, w=세기
	DirectX::XMFLOAT4   camPos;
	DirectX::XMFLOAT4   gridMin;
	DirectX::XMFLOAT4   gridMax;
	DirectX::XMFLOAT4   gridDim;   // x,y,z=격자수, w=레이수
	DirectX::XMFLOAT4   giParams;  // x=GI세기, y=frame, z=ambient
};

// ───────────────────────────────────────────────────────────
// D3D12Device — DX11 엔진의 Graphics 에 대응하는 DX12 디바이스/스왑체인 래퍼.
// Phase 0~3 + DDGI(SH/가시성/다중바운스) + .mesh 모델 + 스키닝 애니메이션.
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
	void CreateASBuffers();   // BLAS/TLAS/스크래치/인스턴스 버퍼 생성 + 초기 빌드 (1회)
	void RecordBuildAS();     // BLAS+TLAS 빌드를 현재 커맨드리스트에 기록 (정적=1회, 스키닝=매프레임)
	ComPtr<ID3D12Resource> CreateDefaultBuffer(UINT64 size, D3D12_RESOURCE_STATES state, bool allowUAV);

	// 스키닝 애니메이션 (CPU) — 매 프레임 본 행렬 계산 → 정점 스키닝 → VB 갱신
	void UpdateAnimation();

	// 텍스처 (디퓨즈 PNG → DX12 텍스처 + SRV 힙)
	void CreateTextureResources();

	// Phase 3 (DDGI)
	void CreateGI();    // 프로브 버퍼 + 컴퓨트 루트시그/PSO
	void DispatchGI();  // 매 프레임 RT 레이로 프로브 irradiance 갱신
	void Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to);

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

	// 스키닝 애니메이션
	std::vector<LoadedBone>           _bonesData;
	std::vector<SkinVtx>              _skinSrc;       // 모델 원본 정점(바인드, 블렌드 포함)
	AnimClip                          _clip;
	bool                              _animated = false;
	std::vector<Vtx>                  _cpuVerts;      // 합본(모델+바닥) CPU 미러 — 매프레임 모델부 갱신
	uint32                            _modelVtxCount = 0;
	float                             _modelScale = 1.f;
	DirectX::XMFLOAT3                 _modelOffset{ 0, 0, 0 };
	void*                             _vbMapped = nullptr; // VB 영속 매핑
	UINT64                            _flushValue = 0;
	uint32                            _modelIndexCount = 0; // 모델 인덱스 수(바닥 제외) — 텍스처 드로우 분리용

	// PBR 텍스처 — 다중 머티리얼: 머티리얼 슬롯당 3개(디퓨즈/노멀/스펙) 연속 SRV 힙.
	// 드로우 시 서브메시의 matSlot 으로 테이블 핸들을 slot*3 만큼 오프셋.
	std::vector<ComPtr<ID3D12Resource>> _matResources; // 슬롯×3 (생존 유지)
	ComPtr<ID3D12Resource>            _whiteTex;        // 폴백(1×1 흰색)
	ComPtr<ID3D12DescriptorHeap>      _srvHeap;
	UINT                              _srvInc = 0;       // 디스크립터 증가량
	UINT                              _matCount = 0;     // 머티리얼 슬롯 수
	std::vector<SubMesh>              _submeshes;        // 머티리얼별 인덱스 구간 + matSlot(materialName→슬롯)
	std::vector<uint32>               _subMatSlot;       // _submeshes[i] 의 머티리얼 슬롯 인덱스
	bool                              _hasTexture = false;

	// Phase 3 — DDGI 프로브 볼륨 (DC irradiance, 컴퓨트 RT 수집)
	static const UINT PROBE_X = 10;
	static const UINT PROBE_Y = 5;
	static const UINT PROBE_Z = 10;
	static const UINT PROBE_COUNT = PROBE_X * PROBE_Y * PROBE_Z;
	ComPtr<ID3D12RootSignature>       _giRootSig;
	ComPtr<ID3D12PipelineState>       _giPSO;
	ComPtr<ID3D12Resource>            _probes;       // float3[PROBE_COUNT] (UAV/SRV)
	D3D12_RESOURCE_STATES             _probeState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_RAYTRACING_TIER             _dxrTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	std::wstring                      _adapterName;
	float                             _time = 0.f;
};
