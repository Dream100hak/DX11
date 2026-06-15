#pragma once
#include "Common.h"
#include "MeshLoader.h"
#include "ImGuiDx12.h"
#include <string>

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
	DirectX::XMFLOAT4X4 invVP;     // 역 뷰프로젝션 (스카이)
	DirectX::XMFLOAT4   pointPos;  // xyz 점광원 위치, w 반경
	DirectX::XMFLOAT4   pointColor;// rgb 색, w 세기
	DirectX::XMFLOAT4   matParams; // x metallic, y roughness, z emissive, w albedoTint
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
	void LoadModelFromFile(const std::wstring& meshPath); // 런타임 모델 교체 (에셋 더블클릭)
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
	ComPtr<ID3D12PipelineState>       _skyPSO;     // 절차적 스카이박스
	DXGI_FORMAT                       _sceneFmt = DXGI_FORMAT_R8G8B8A8_UNORM; // 씬 RT 포맷 (S3에서 HDR로)
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
	std::wstring                      _modelDir;       // 현재 모델 폴더(.mmat/.mat/텍스처 기준)
	std::wstring                      _modelStem;      // 현재 모델 스템(예: Archer)
	std::wstring                      _modelLabel = L"Archer"; // 하이어라키/인스펙터 표시명
	std::wstring                      _pendingModel;   // 더블클릭 로드 대기 경로 (다음 프레임 처리)
	DirectX::XMFLOAT4X4               _modelMatrix;    // 기즈모 트랜스폼 (정점에 매프레임 적용 → RT 일치)
	bool                              _modelMatrixInit = false;

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
	static const UINT PROBE_OCT = 8; // 프로브당 옥타헤드럴 depth 해상도 (셰이더 OCT 와 일치)
	ComPtr<ID3D12RootSignature>       _giRootSig;
	ComPtr<ID3D12PipelineState>       _giPSO;
	ComPtr<ID3D12Resource>            _probes;       // ProbeSH[PROBE_COUNT] (UAV/SRV)
	ComPtr<ID3D12Resource>            _probeDepth;   // float2[PROBE_COUNT×OCT²] mean/mean² (UAV/SRV)
	D3D12_RESOURCE_STATES             _probeState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES             _probeDepthState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_RAYTRACING_TIER             _dxrTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	std::wstring                      _adapterName;
	float                             _time = 0.f;

	// 자유 비행 카메라 (WASD 이동 / 우클릭 드래그 마우스 룩 / Q·E 상하 / Shift 가속)
	void                              UpdateCamera(float dt);
	HWND                              _hwnd = nullptr;
	DirectX::XMFLOAT3                 _camPos{ 3.4f, 2.4f, -4.6f };
	float                             _camYaw = -0.637f;   // 초기 시선(원점 부근) 방향
	float                             _camPitch = -0.232f;
	bool                              _rmbDown = false;
	POINT                             _lastCursor{ 0, 0 };

	// ── 에디터 UI (ImGui) — 도킹 + Inspector / FolderContents / Scene(RT 이미지) ──
	void                              InitEditor();
	void                              BuildUI();           // ImGui::NewFrame ~ Render 사이 패널 구성
	void                              DrawHierarchy();
	void                              DrawInspector();
	void                              DrawFolderContents();
	void                              DrawSceneView();
	enum class SelEntity { Model, Floor, Sun, DDGI, Camera };
	SelEntity                         _sel = SelEntity::Model; // 하이어라키 선택 → 인스펙터 표시 대상
	void                              CreateSceneRT(UINT w, UINT h); // 씬 오프스크린 RT/깊이 (재)생성
	ImGuiDx12                         _imgui;
	bool                              _editorReady = false;

	// 씬 오프스크린 RT (ImGui::Image 로 "Scene" 도킹 탭에 표시)
	ComPtr<ID3D12Resource>            _sceneRT;
	ComPtr<ID3D12Resource>            _sceneDepth;
	ComPtr<ID3D12DescriptorHeap>      _sceneRtvHeap;
	ComPtr<ID3D12DescriptorHeap>      _sceneDsvHeap;
	D3D12_RESOURCE_STATES             _sceneRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	UINT                              _sceneW = 1280, _sceneH = 720;       // 현재 RT 크기
	UINT                              _pendingSceneW = 0, _pendingSceneH = 0; // Scene 창 컨텐츠 크기
	uint64                            _sceneTexId = 0;     // ImGui::Image 핸들
	bool                              _sceneHovered = false, _sceneFocused = false;
	DirectX::XMFLOAT4X4               _viewM, _projM;      // ImGuizmo 용 (Render 에서 갱신)
	int                               _gizmoOp = 7;        // ImGuizmo::TRANSLATE (헤더에 ImGuizmo 미포함 → int)
	// 인스펙터에서 편집하는 라이팅/GI 파라미터 (Render 가 매 프레임 SceneCB 에 반영)
	float                             _lightIntensity = 1.2f;
	bool                              _lightAnimate = true;
	float                             _lightAngle = 0.f;
	float                             _giStrength = 0.45f;
	float                             _ambient = 0.03f;
	// 점 조명 (S6)
	bool                              _pointOn = false;
	DirectX::XMFLOAT3                 _pointPos{ 1.5f, 2.2f, 0.0f };
	DirectX::XMFLOAT3                 _pointColor{ 1.0f, 0.6f, 0.3f };
	float                             _pointIntensity = 4.0f;
	float                             _pointRadius = 7.0f;
	// 편집 머티리얼 (S5)
	float                             _matMetallic = 0.0f, _matRoughness = 0.5f, _matEmissive = 0.0f, _matTint = 1.0f;
	// 뷰포트 토글 (S10)
	bool                              _showGrid = true, _showSky = true, _bloomOn = true, _wireframe = false;
	// FolderContents 상태
	std::wstring                      _assetRoot;          // Resources/Assets 절대경로
	std::wstring                      _curDir;             // 현재 탐색 폴더
	std::wstring                      _selectedAsset;      // 선택 파일(인스펙터 표시)
};
