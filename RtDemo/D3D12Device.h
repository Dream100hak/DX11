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
	DirectX::XMFLOAT4   sunColor;
	DirectX::XMFLOAT4   fog;
	DirectX::XMFLOAT4   grade;
	DirectX::XMFLOAT4   skyZenith;
	DirectX::XMFLOAT4   skyHorizon;
	DirectX::XMFLOAT4   dbg;
	DirectX::XMFLOAT4   spotPos;
	DirectX::XMFLOAT4   spotDir;
	DirectX::XMFLOAT4   spotColor;
	DirectX::XMFLOAT4   tint;
	DirectX::XMFLOAT4   ptPos[4];
	DirectX::XMFLOAT4   ptCol[4];
	DirectX::XMFLOAT4   floorMat;
	DirectX::XMFLOAT4   ao;
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
	ComPtr<ID3D12PipelineState>       _gridPSO;    // 무한 씬 그리드
	ComPtr<ID3D12PipelineState>       _outlinePSO; // 선택 아웃라인(인버티드 헐)
	ComPtr<ID3D12PipelineState>       _wirePSO;    // 와이어프레임 토글
	ComPtr<ID3D12PipelineState>       _probePSO;   // DDGI 프로브 점 시각화
	ComPtr<ID3D12PipelineState>       _fxaaPSO;    // FXAA
	ComPtr<ID3D12PipelineState>       _dbgPSO;     // 디버그 라인(본/AABB/콘/아이콘)
	ComPtr<ID3D12Resource>            _dbgVB;      // 디버그 라인 동적 VB
	void*                             _dbgMapped = nullptr;
	UINT                              _dbgCap = 0;
	std::vector<DirectX::XMFLOAT3>    _boneWorld;  // 본 월드 위치(스키닝 시 채움)
	void                              DrawDebugLines(); // 본/AABB/콘/아이콘 라인 빌드+드로우
	DXGI_FORMAT                       _sceneFmt = DXGI_FORMAT_R16G16B16A16_FLOAT; // 씬 RT(HDR)

	// 포스트프로세스 (S3 톤맵 / S4 블룸) — 공용 SRV 힙 + 루트시그
	void CreatePostFX();
	ComPtr<ID3D12RootSignature>       _postRootSig;
	ComPtr<ID3D12PipelineState>       _tonemapPSO;
	ComPtr<ID3D12DescriptorHeap>      _postSrvHeap; // slot0 HDR씬 / slot1 bloom / 2~ bloom 밉
	UINT                              _postSrvInc = 0;
	ComPtr<ID3D12Resource>            _sceneLDR;    // 톤맵 결과
	ComPtr<ID3D12Resource>            _sceneLDR2;   // FXAA 결과 (ImGui 표시 후보)
	D3D12_RESOURCE_STATES             _sceneLDRState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES             _sceneLDR2State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	float                             _exposure = 1.0f;
	// 블룸 (S4) — 반해상도 ping-pong
	ComPtr<ID3D12PipelineState>       _brightPSO, _blurPSO;
	ComPtr<ID3D12Resource>            _bloomA, _bloomB;
	ComPtr<ID3D12DescriptorHeap>      _bloomRtvHeap;
	D3D12_RESOURCE_STATES             _bloomAState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES             _bloomBState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	UINT                              _bloomW = 0, _bloomH = 0;
	bool                              _bloomReady = false; // S4 에서 true
	float                             _bloomIntensity = 0.6f;
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
	std::wstring                      _pendingModel;   // 더블클릭/씬로드 모델 경로 (다음 프레임 처리)
	DirectX::XMFLOAT4X4               _modelMatrix;    // 기즈모 트랜스폼 (정점에 매프레임 적용 → RT 일치)
	bool                              _modelMatrixInit = false;
	DirectX::XMFLOAT4X4               _pendingMatrix;  // 씬 로드 시 모델 로드 후 적용할 트랜스폼
	bool                              _hasPendingMatrix = false;
	void                              SaveScene();     // .rtscene 저장
	void                              LoadScene();     // .rtscene 로드

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
	void                              DrawLog();
	enum class SelEntity { Model, Floor, Sun, DDGI, Camera, Point, Spot, Post };
	SelEntity                         _sel = SelEntity::Model; // 하이어라키 선택 → 인스펙터 표시 대상
	void                              CreateSceneRT(UINT w, UINT h); // 씬 오프스크린 RT/깊이 (재)생성
	ImGuiDx12                         _imgui;
	bool                              _editorReady = false;

	// 씬 오프스크린 RT (ImGui::Image 로 "Scene" 도킹 탭에 표시)
	ComPtr<ID3D12Resource>            _sceneRT;
	ComPtr<ID3D12Resource>            _sceneDepth;
	D3D12_RESOURCE_STATES             _sceneDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	ComPtr<ID3D12DescriptorHeap>      _sceneRtvHeap;
	ComPtr<ID3D12DescriptorHeap>      _sceneDsvHeap;
	D3D12_RESOURCE_STATES             _sceneRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	UINT                              _sceneW = 1280, _sceneH = 720;       // 현재 RT 크기
	UINT                              _pendingSceneW = 0, _pendingSceneH = 0; // Scene 창 컨텐츠 크기
	uint64                            _sceneTexId = 0;     // ImGui::Image 핸들
	bool                              _sceneHovered = false, _sceneFocused = false;
	DirectX::XMFLOAT4X4               _viewM, _projM;      // ImGuizmo 용 (Render 에서 갱신)
	int                               _gizmoOp = 7;        // ImGuizmo::TRANSLATE (헤더에 ImGuizmo 미포함 → int)
	DirectX::XMFLOAT3                 _modelMin{}, _modelMax{}; // 모델 월드 AABB (클릭 픽킹)
	void                              PickAt(float u, float v); // 씬뷰 클릭 → 레이 픽킹
	// 인스펙터에서 편집하는 라이팅/GI 파라미터 (Render 가 매 프레임 SceneCB 에 반영)
	float                             _lightIntensity = 1.2f;
	bool                              _lightAnimate = true;
	float                             _lightAngle = 0.f;
	float                             _giStrength = 0.45f;
	float                             _ambient = 0.03f;
	// 점 조명 (S6)
	bool                              _pointOn = true;
	DirectX::XMFLOAT3                 _pointPos{ 1.6f, 1.6f, 1.2f };
	DirectX::XMFLOAT3                 _pointColor{ 1.0f, 0.6f, 0.3f };
	float                             _pointIntensity = 4.0f;
	float                             _pointRadius = 7.0f;
	// 편집 머티리얼 (S5)
	float                             _matMetallic = 0.0f, _matRoughness = 0.5f, _matEmissive = 0.0f, _matTint = 1.0f;
	// 뷰포트 토글 (S10)
	bool                              _showGrid = true, _showSky = true, _bloomOn = true, _wireframe = false;

	// ── 20종 확장 상태 ──
	float                             _fov = 55.f, _moveSpeed = 3.5f, _fastMul = 2.6f; // T1
	bool                              _gizmoLocal = false, _snapOn = false; float _snapT = 0.5f, _snapR = 15.f, _snapS = 0.1f; // T2
	DirectX::XMFLOAT3                 _sunColor{ 1.0f, 0.96f, 0.88f }; float _envIntensity = 1.0f; // T5
	float                             _bloomThreshold = 1.0f; // T6 (셰이더 전달)
	int                               _tonemapOp = 0; // T7: 0 ACES / 1 Reinhard / 2 Filmic
	float                             _contrast = 1.0f, _saturation = 1.0f, _temperature = 0.0f, _vignette = 0.25f; // T8
	DirectX::XMFLOAT3                 _fogColor{ 0.55f, 0.62f, 0.72f }; float _fogDensity = 0.0f; // T9
	bool                              _fxaaOn = true; // T10
	float                             _shadowSoft = 0.0f; // T11 (소프트 그림자 반경)
	bool                              _reflectOn = false; float _reflectStrength = 0.5f; // T12
	bool                              _spotOn = false; DirectX::XMFLOAT3 _spotPos{ -1.6f, 2.4f, 0.0f }, _spotColor{ 0.5f, 0.7f, 1.0f }; // T13
	float                             _spotIntensity = 6.0f, _spotRadius = 9.0f, _spotConeDeg = 28.0f; DirectX::XMFLOAT3 _spotDir{ 0.4f, -1.0f, 0.0f };
	static const int                  MAX_PT = 4; // T14 다중 점광원
	int                               _ptCount = 1; DirectX::XMFLOAT4 _ptPosArr[MAX_PT]{}; DirectX::XMFLOAT4 _ptColArr[MAX_PT]{};
	bool                              _probeViz = false; // T15
	int                               _debugView = 0;    // T16: 0 none/1 albedo/2 normal/3 depth/4 GI
	bool                              _animPaused = false; float _animSpeed = 1.0f, _animTimeAcc = 0.0f; // T17
	std::vector<std::wstring>         _clips; int _curClip = 0; // T18
	bool                              _wantShot = false; // T19
	DirectX::XMFLOAT3                 _skyZenith{ 0.13f, 0.22f, 0.44f }, _skyHorizon{ 0.52f, 0.60f, 0.72f }; float _sunSize = 900.f; // T20
	DirectX::XMFLOAT3                 _diffuseTint{ 1.0f, 1.0f, 1.0f }; // T4 RGB 틴트
	void                              SaveScreenshot(); // T19
	void                              ScanClips();      // T18

	// ── 추가 20종(U) ──
	bool                              _aoOn = false; float _aoIntensity = 1.0f, _aoRadius = 0.6f; // U1 RT AO
	bool                              _dofOn = false; float _dofFocus = 6.0f, _dofRange = 4.0f;   // U2 DOF
	bool                              _volOn = false; float _volStrength = 0.5f;                  // U3 갓레이
	bool                              _autoExp = false; float _expScale = 1.0f, _expTarget = 0.5f; // U4 자동노출
	float                             _chroma = 0.0f, _grain = 0.0f, _sharpen = 0.0f;             // U5/U6/U7
	DirectX::XMFLOAT3                 _floorColor{ 0.85f, 0.13f, 0.11f }; float _floorMetallic = 0.0f, _floorRough = 0.6f; // U9
	bool                              _turntable = false; float _turnSpeed = 0.4f, _turnAngle = 0.0f; // U14
	float                             _renderScale = 1.0f;  // U15
	bool                              _showBones = false;   // U10
	bool                              _showAABB = false;    // U11
	bool                              _showLightIcons = true; // U12
	bool                              _showSpotCone = true; // U13
	float                             _groundSize = 6.0f;   // U19
	std::vector<std::string>          _log;                 // U16 로그
	void                              Log(const std::string& m);
	struct Snapshot { DirectX::XMFLOAT4X4 m; float met, rough, emis, tint; DirectX::XMFLOAT3 dt; }; // U17
	std::vector<Snapshot>             _undo, _redo;
	void                              PushUndo(); void DoUndo(); void DoRedo();
	float                             _frameTimes[120]{}; int _frameIdx = 0; // U18

	// FolderContents 상태
	std::wstring                      _assetRoot;          // Resources/Assets 절대경로
	std::wstring                      _curDir;             // 현재 탐색 폴더
	std::wstring                      _selectedAsset;      // 선택 파일(인스펙터 표시)
};
