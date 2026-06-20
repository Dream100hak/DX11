#pragma once
#include "Common.h"
#include "MeshLoader.h"
#include "ImGuiDx12.h"
#include "DebugDraw.h"
#include "Thumbnail.h"
#include "Ddgi.h"
#include "PostFX.h"
#include "FlyCamera.h"
#include "ModelScene.h"
#include "Scene.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "EditorManager.h"
#include "EditorWindows.h"
#include <string>

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
	DirectX::XMFLOAT4   ptPos[16];
	DirectX::XMFLOAT4   ptCol[16];
	DirectX::XMFLOAT4   floorMat;
	DirectX::XMFLOAT4   ao;
	DirectX::XMFLOAT4   shade;
	DirectX::XMFLOAT4   rimColor;
	DirectX::XMFLOAT4   gridParams;
	DirectX::XMFLOAT4   outline;
	DirectX::XMFLOAT4   decal;
	DirectX::XMFLOAT4   decalCol;
	DirectX::XMFLOAT4   extra;
	DirectX::XMFLOAT4   fog2;       // 높이 안개 (시작Y/낙폭/on)
	DirectX::XMFLOAT4   decalArr[8];
	DirectX::XMFLOAT4   decalColArr[8];
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
	void OnResize(UINT width, UINT height); // 창 크기 변경 → 스왑체인 백버퍼 재생성 (WM_SIZE)

	static D3D12Device* Get() { return s_main; } // 전역 접근(컴포넌트가 백포인터 없이 디바이스 도달) — DX12판 GRAPHICS

	// Engine GRAPHICS->GetDevice()/GetDeviceContext() 접근 관례 (DX12 적응) — 물리 분리 대신 접근 API
	ID3D12Device5*              Device()     const { return _device.Get(); }
	ID3D12CommandQueue*         Queue()      const { return _queue.Get(); }
	ID3D12GraphicsCommandList4* Cmd()        const { return _cmdList.Get(); }
	UINT                        FrameIndex() const { return _frameIndex; }
	D3D12_GPU_VIRTUAL_ADDRESS   FrameCB()    const { return _cb[_frameIndex]->GetGPUVirtualAddress(); }

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
	void DumpDeviceRemoved(); // DRED breadcrumb/page-fault 덤프

	// Phase 1
	void CreateDepthBuffer();
	void CreateRootSignature();
	void CreatePipeline();
	void CreateConstantBuffers();
	ComPtr<ID3D12Resource> CreateUploadBuffer(const void* data, size_t size); // 단순 업로드힙 버퍼
	ComPtr<ID3D12Resource> CreateDefaultBuffer(UINT64 size, D3D12_RESOURCE_STATES state, bool allowUAV);

	// 모델/스키닝/텍스처/가속구조는 ModelScene 가 소유 (_scene). 렌더러 컴포넌트도 friend 로 접근.
	friend class ModelScene;
	friend class ModelRenderer;
	friend class MeshRenderer;
	friend class ModelAnimator;
	friend class SkyRenderer;
	friend class GridRenderer;
	friend class Foliage;
	friend class EditorManager;

	// Phase 3 (DDGI) — 프로브/컴퓨트는 Ddgi 클래스가 소유. CreateGI 는 셰이더 컴파일 후 위임
	void CreateGI();
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
	// 스카이박스 큐브맵 (.dds) — 절차 하늘 대체(토글)
	ComPtr<ID3D12Resource>            _skyCube;
	ComPtr<ID3D12DescriptorHeap>      _skyCubeHeap; // t2~t4 테이블용(큐브 SRV)
	bool                              _skyCubemapOn = false;
	bool                              LoadSkyCubemap(const std::wstring& ddsPath); // DDS 큐브맵 로드 + SRV
	ComPtr<ID3D12PipelineState>       _gridPSO;    // 무한 씬 그리드
	ComPtr<ID3D12PipelineState>       _outlinePSO; // 선택 아웃라인(인버티드 헐)
	ComPtr<ID3D12PipelineState>       _wirePSO;    // 와이어프레임 토글
	ComPtr<ID3D12PipelineState>       _probePSO;   // DDGI 프로브 점 시각화
	ComPtr<ID3D12PipelineState>       _particlePSO; // GPU 인스턴스드 빌보드 파티클
	ComPtr<ID3D12Resource>            _partInst;    // per-frame 인스턴스 업로드 버퍼
	void*                             _partInstMapped = nullptr;
	UINT                              _partInstCap = 0; // 바이트 용량
	void                              RenderParticles(const RenderContext& ctx); // 씬의 ParticleSystem 입자를 빌보드로
	// 터레인 GPU 테셀레이션 (OFF 기본 — 토글)
	ComPtr<ID3D12PipelineState>       _tessPSO, _tessWirePSO;
	bool                              _tessTerrain = false;
	float                             _tessFactor = 16.f;
	ComPtr<ID3D12Resource>            _tessCP, _tessHeights; // 컨트롤포인트 VB / 하이트맵 StructuredBuffer (per-frame 업로드)
	void*                             _tessCPMapped = nullptr; void* _tessHeightsMapped = nullptr;
	UINT                              _tessCPCap = 0, _tessHeightsCap = 0;
	void                              RenderTessTerrain(const RenderContext& ctx); // 선택/첫 터레인을 테셀레이션으로
	// 물 평면 (OFF 기본 — 토글)
	ComPtr<ID3D12PipelineState>       _waterPSO;
	bool                              _waterOn = false;
	float                             _waterLevel = 0.2f, _waterSize = 60.f, _waterGrid = 80.f;
	void                              RenderWater(const RenderContext& ctx);
	DebugDraw                         _debugDraw;  // 디버그 라인 렌더러(본/AABB/콘/아이콘/파티클)
	void                              DrawDebugLines(); // 에디터 상태 → 라인 빌드 → _debugDraw 드로우
	DXGI_FORMAT                       _sceneFmt = DXGI_FORMAT_R16G16B16A16_FLOAT; // 씬 RT(HDR)

	// 포스트프로세스 (블룸/톤맵/FXAA) — PostFX 클래스가 RT/힙/PSO 소유
	PostFX                            _postfx;
	float                             _exposure = 1.0f;      // 에디터 파라미터(인스펙터/씬저장) → TonemapParams
	float                             _bloomIntensity = 0.6f; // 〃
	ComPtr<ID3D12Resource>            _cb[FRAME_COUNT];
	void*                             _cbMapped[FRAME_COUNT] = {};
	UINT64                            _flushValue = 0;

	// 모델/스키닝/텍스처/가속구조 일체 — ModelScene 클래스
	ModelScene                        _scene;
	// 씬 그래프(이관 시작) — 모델을 GameObject(ModelRenderer)로 보유, 렌더 루프가 순회
	shared_ptr<Scene>                 _gameScene;
	shared_ptr<GameObject>            _modelObj;
	shared_ptr<GameObject>            _camObj;  // 에디터 카메라 GameObject (Camera 컴포넌트)
	shared_ptr<GameObject>            _sunObj, _ptObj, _spotObj; // 라이트 GameObject (CB 소스)
	void                              BuildGameScene(); // 모델 + 카메라 + 라이트 GameObject 구성
	void                              SyncLights();     // 스칼라 → Light 컴포넌트 동기화 (CB 가 컴포넌트 읽음)
	// 씬그래프 편집 (하이어라키 컨텍스트 메뉴/단축키)
	shared_ptr<GameObject>            SpawnMeshObject(const std::wstring& name, const vector<Vtx>& v, const vector<uint32>& idx, const Vec3& pos, MeshPrim prim = MeshPrim::None, bool autoName = true);
	shared_ptr<GameObject>            SpawnEmpty(const std::wstring& name, const Vec3& pos);
	shared_ptr<GameObject>            SpawnAnimatedModel(const std::wstring& meshPath, const Vec3& pos); // ModelAnimator
	void                              ConvertFbxDialog(); // File > Convert FBX... (ufbx → .mesh/.clip/.mat 변환 후 스폰)
	void                              SaveSelectedAsPrefab(); // 선택 GO → .prefab 파일
	void                              InstantiatePrefab();    // .prefab 파일 → 씬에 스폰
	shared_ptr<GameObject>            SpawnTerrain(int gridN, float cellSize); // Terrain + MeshRenderer GameObject
	void                              SpawnShowcase(); // 데모 씬 일괄 생성(터레인+물+식생+파티클+라이트)
	// 터레인 편집 (씬뷰 브러시) — Terrain 선택 + Edit 토글 시 좌드래그로 스컬프트
	bool                              _terrainEdit = false;
	int                               _terrainBrush = 0;     // 0 Raise/1 Lower/2 Smooth/3 Flatten/4 Paint
	float                             _terrainRadius = 6.f;
	float                             _terrainStrength = 8.f; // 초당 변화량(m)
	float                             _terrainFlatten = 0.f;
	Vec3                              _terrainPaintColor{ 0.5f, 0.4f, 0.25f }; // Paint 브러시 색(흙)
	// 식생(Foliage) 생성 파라미터 (인스펙터 입력) — Generate 시 Foliage 렌더러 GameObject 생성/갱신
	int                               _folGrass = 4000;
	int                               _folTree = 60;
	float                             _folSize = 0.4f;
	int                               _folSeed = 1337;
	void                              GenerateFoliage(const shared_ptr<GameObject>& terrainObj); // 터레인용 식생 GameObject 생성/재생성
	Vec3                              _terrainCursor{};       // 마지막 브러시 월드 히트(기즈모/오버레이용)
	bool                              _terrainCursorValid = false;
	void                              TerrainBrushAt(float u, float v, bool apply); // 씬뷰 uv → 레이 → (apply 시)스컬프트
	void                              FocusCameraOn(const Vec3& target); // 카메라를 대상 지점으로 이동 + 시선 정렬
	void                              FrameAll();                        // 모든 비-내부 오브젝트를 화면에 담도록 카메라 프레이밍 (Home)
	Vec3                              SpawnPoint(); // 카메라 앞 4m 지점 (스폰 위치)
	shared_ptr<GameObject>            SpawnLight(int type, const std::wstring& name, const Vec3& pos); // 0 Dir/1 Point/2 Spot
	void                              DeleteSelectedObject();    // _selectedGO 삭제 (에디터 내부/모델 보호)
	void                              DuplicateSelectedObject(); // _selectedGO(+멀티셀렉트) 복제
	void                              DuplicateObject(const shared_ptr<GameObject>& source); // 단일 GO 복제
	void                              RemoveObject(const shared_ptr<GameObject>& obj); // 부모분리+자식승격+씬제거
	void                              NewScene();                // 씬그래프 비우기 + 파라미터 리셋
	void                              GroupSelected();           // 선택(+멀티) → 무게중심 빈 부모 아래로 그룹화 (Ctrl+G)
	void                              SnapSelectedToGrid();      // 선택(+멀티) 위치를 이동 스냅(_snapT) 격자에 정렬
	int                               _spawnCounter = 0;         // 고유 이름 접미사
	struct DecalItem { Vec3 pos{ 0,0,0 }; float radius = 2.f; Vec3 color{ 0.8f,0.1f,0.1f }; }; // 다중 데칼(상향 투영)
	std::vector<DecalItem>            _decals;
	bool                              _heightFog = false; float _fogHeight = 3.f, _fogFalloff = 0.3f; // 높이 안개
	std::vector<int64>                _selIds;                   // 추가 선택(멀티셀렉트) — primary=_selectedGO 제외 id 목록
	int64                             _anchorId = -1;            // Shift 범위 선택 기준(마지막 단일 클릭)
	bool                              IsMultiSelected(int64 id) const { for (int64 s : _selIds) if (s == id) return true; return false; }
	// _scene 재로드 예약 (바닥 터레인 토글/그라운드 사이즈 — 다음 프레임 GPU 유휴 시점에 처리)
	std::wstring                      _pendingModel;
	void                              SaveScene();     // .rtscene 저장 (quick)
	void                              LoadScene();     // .rtscene 로드 (quick)
	void                              SaveSceneTo(const std::wstring& path);
	void                              LoadSceneFrom(const std::wstring& path);
	void                              TogglePlay();    // Play=스냅샷 저장 / Stop=스냅샷 복원
	int                               ClearDynamicObjects(); // 스폰 오브젝트 제거 (NewScene/Stop 공용)
	bool                              _playing = false;
	DirectX::XMFLOAT3                 _playCamPos{}; float _playCamYaw = 0, _playCamPitch = 0; // Play 진입 시 에디터 카메라 포즈(Stop 복원)

	// Phase 3 — DDGI 프로브 볼륨 (Ddgi 클래스가 프로브 버퍼 + 컴퓨트 GI 디스패치 소유)
	Ddgi                              _ddgi;
	// RT 집계 지오메트리 — 모든 TLAS 인스턴스의 월드 정점/인덱스를 한 버퍼로 모아
	// gather 셰이더가 InstanceID+{vbBase,ibBase}(t3 _rtMeta)로 자기 인스턴스 지오메트리를 페치.
	// DEFAULT 힙: 각 인스턴스의 GPU VB/IB 를 CopyBufferRegion 으로 집계(CPU memcpy 폐지 — GPU 스키닝 호환).
	ComPtr<ID3D12Resource>            _rtVB; UINT _rtVBCap = 0; D3D12_RESOURCE_STATES _rtVBState = D3D12_RESOURCE_STATE_COPY_DEST; // 정점(Vtx)
	ComPtr<ID3D12Resource>            _rtIB; UINT _rtIBCap = 0; D3D12_RESOURCE_STATES _rtIBState = D3D12_RESOURCE_STATE_COPY_DEST; // 인덱스(uint32)
	ComPtr<ID3D12Resource>            _rtMeta; void* _rtMetaMapped = nullptr; UINT _rtMetaCap = 0; // {vbBase,ibBase}×인스턴스(업로드)
	struct RtGeomSrc { ID3D12Resource* vb; ID3D12Resource* ib; uint32 vcount; uint32 icount; }; // 집계 소스(인스턴스별 GPU 버퍼)
	void                              BuildRtGeometry(const std::vector<RtGeomSrc>& geom, ID3D12GraphicsCommandList4* cmd); // GPU 복사로 집계

	D3D12_RAYTRACING_TIER             _dxrTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	std::wstring                      _adapterName;
	std::wstring                      _shaderDir; // exe\Shaders (런타임 .hlsl 로드 경로 — CreatePipeline/CreateGI)
	float                             _time = 0.f;

	// 자유 비행 카메라 — FlyCamera 클래스가 상태/입력/뷰·프로젝션 소유
	FlyCamera                         _camera;
	HWND                              _hwnd = nullptr;

	// ── 에디터 UI (ImGui) — EditorManager + EditorWindow 프레임워크(EditorTool 대응) ──
	// 각 Draw* 는 해당 윈도우 클래스가 Update() 에서 렌더한다(friend).
	EditorManager                     _editor;
	friend class MainMenuBarWindow; friend class SceneViewWindow; friend class HierarchyWindow;
	friend class InspectorWindow; friend class ProjectWindow; friend class FolderContentsWindow; friend class LogPanelWindow;
	void                              InitEditor();
	void                              BuildUI();           // ImGui NewFrame ~ Render (EditorManager 위임)
	void                              DrawMainMenuBar();
	void                              DrawHierarchy();
	void                              DrawInspector();
	void                              DrawGameObjectInspector(const shared_ptr<GameObject>& go); // 컴포넌트 기반
	void                              DrawFolderContents();
	void                              DrawSceneView();
	void                              DrawLog();
	void                              DrawProject();
	enum class SelEntity { Model, Floor, Sun, DDGI, Camera, Point, Spot, Post };
	SelEntity                         _sel = SelEntity::Post;  // 하이어라키 선택 → 인스펙터 표시 대상 (모델은 GameObject 로 분리됨)
	shared_ptr<GameObject>            _selectedGO;             // GameObject 기반 선택 (있으면 인스펙터가 컴포넌트 표시)
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

	// ── Game 뷰 — 게임 카메라(비-에디터 Camera GameObject) 시점 별도 RT ──
	ComPtr<ID3D12Resource>            _gameCB; void* _gameCBMapped = nullptr;
	ComPtr<ID3D12Resource>            _gameRT, _gameDepth;
	ComPtr<ID3D12DescriptorHeap>      _gameRtvHeap, _gameDsvHeap;
	D3D12_RESOURCE_STATES             _gameRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES             _gameDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	UINT                              _gameW = 640, _gameH = 360, _pendingGameW = 0, _pendingGameH = 0;
	uint64                            _gameTexId = 0;
	SceneCB                           _cbCache{}; // 직전 에디터 CB (게임 패스 베이스)
	PostFX                            _gamePostfx;
	bool                              _gameWindowOpen = true;
	void                              CreateGameRT(UINT w, UINT h);
	void                              RenderGameView(); // 게임 카메라 → _gameRT (Scene 패스 후)
	void                              DrawGameView();   // "Game" 도킹 창
	DirectX::XMFLOAT4X4               _viewM, _projM;      // ImGuizmo 용 (Render 에서 갱신)
	int                               _gizmoOp = 7;        // ImGuizmo::TRANSLATE (헤더에 ImGuizmo 미포함 → int)
	void                              PickAt(float u, float v); // 씬뷰 클릭 → 레이 픽킹 (모델 AABB = _scene._modelMin/Max)
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
	// 뷰포트 토글 (S10)
	bool                              _showGrid = true, _showSky = true, _bloomOn = true, _wireframe = false;
	bool                              _showFloor = true;    // 바닥 표시(래스터+RT). 끄면 바닥 그림자/GI 도 빠짐
	bool                              _showStats = false;   // 씬뷰 통계 오버레이(FPS 그래프 + 카운트)
	bool                              _resetLayout = false; // View > Reset Layout (EditorManager 가 다음 프레임 도킹 재구성)
	bool                              _frustumCull = false; // 절두체 컬링(Opaque) — 기본 off(안전), 인스펙터 토글
	static D3D12Device*               s_main;               // Get() 전역 접근용

	// ── 20종 확장 상태 ── (카메라 FOV/이동속도/Near·Far/오빗/북마크는 FlyCamera 로 이동)
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
	static const int                  MAX_PT = 16; // T14 다중 점광원 (러프 클러스터드 대체 — 캡 상향)
	int                               _ptCount = 1; DirectX::XMFLOAT4 _ptPosArr[MAX_PT]{}; DirectX::XMFLOAT4 _ptColArr[MAX_PT]{};
	bool                              _probeViz = false; // T15
	int                               _debugView = 0;    // T16: 0 none/1 albedo/2 normal/3 depth/4 GI
	bool                              _wantShot = false; // T19
	DirectX::XMFLOAT3                 _skyZenith{ 0.13f, 0.22f, 0.44f }, _skyHorizon{ 0.52f, 0.60f, 0.72f }; float _sunSize = 900.f; // T20
	void                              SaveScreenshot(); // T19

	// ── 추가 20종(U) ──
	bool                              _aoOn = false; float _aoIntensity = 1.0f, _aoRadius = 0.6f; // U1 RT AO
	bool                              _dofOn = false; float _dofFocus = 6.0f, _dofRange = 4.0f;   // U2 DOF
	bool                              _volOn = false; float _volStrength = 0.5f;                  // U3 갓레이
	bool                              _autoExp = false; float _expScale = 1.0f, _expTarget = 0.5f; // U4 자동노출
	float                             _chroma = 0.0f, _grain = 0.0f, _sharpen = 0.0f;             // U5/U6/U7
	DirectX::XMFLOAT3                 _floorColor{ 0.85f, 0.13f, 0.11f }; float _floorMetallic = 0.0f, _floorRough = 0.6f; // U9
	float                             _renderScale = 1.0f;  // U15
	bool                              _showBones = false;   // U10
	bool                              _showAABB = false;    // U11
	bool                              _showLightIcons = true; // U12
	bool                              _showSpotCone = true; // U13
	// U19 _groundSize → Scene._groundSize
	std::vector<std::string>          _log;                 // U16 로그
	void                              Log(const std::string& m);
	void                              ResetDefaults();
	float                             _frameTimes[120]{}; int _frameIdx = 0; // U18

	// ── 3차 20종(V) ──
	bool                              _wantReload = false; // V1 재생성 트리거 (_terrain 토글은 Scene._terrain)
	int                               _toonLevels = 0;          // V2 (0=off)
	float                             _rimPower = 0.0f; DirectX::XMFLOAT3 _rimColor{ 0.3f, 0.55f, 1.0f }; // V3
	bool                              _todOn = false; float _timeOfDay = 0.35f; // V4 시간대
	DirectX::XMFLOAT3                 _outlineColor{ 1.7f, 0.85f, 0.12f }; float _outlineThick = 0.005f; // V5
	float                             _gizmoSize = 0.1f;        // V6
	// V7 Near/Far/Orbit, V8 북마크 → FlyCamera
	float                             _normalIntensity = 1.0f;  // V9
	float                             _lensDistort = 0.0f;      // V10
	float                             _posterize = 0.0f;        // V11 (0=off)
	int                               _filterMode = 0;          // V12 none/sepia/gray/invert
	float                             _ev = 0.0f;               // V13 EV
	bool                              _ptOrbit = false; float _ptOrbitSpeed = 0.6f, _ptOrbitAng = 0.0f; // V14
	float                             _gridCell = 1.0f, _gridFade = 60.0f; // V15
	bool                              _checker = false;         // V16
	int                               _bgMode = 0;              // V17 0 sky / 1 solid
	DirectX::XMFLOAT3                 _bgColor{ 0.06f, 0.07f, 0.10f }; // V17
	bool                              _anamorphic = false;      // V19
	bool                              _hiresShot = false;       // V20 (캡처 후 렌더스케일 복원)

	// ── 4차 10종(W) ──
	struct Particle { DirectX::XMFLOAT3 pos, vel, col; float life; }; // W1
	std::vector<Particle>             _particles; bool _particlesOn = false; int _particleMode = 0; // 0 sparks / 1 snow
	void                              UpdateParticles(float dt);
	bool                              _decalOn = false; DirectX::XMFLOAT3 _decalPos{ 0,0,0 }, _decalColor{ 1.0f, 0.9f, 0.2f }; float _decalRadius = 1.5f; // W2
	float                             _cloudAmt = 0.0f;         // W3
	float                             _letterbox = 0.0f;        // W4
	bool                              _overlay = false;         // W5
	float                             _shadowStrength = 1.0f;   // W6
	float                             _hemiAmbient = 0.0f;      // W7
	bool                              _stars = false;           // W8
	bool                              _flicker = false; float _flickerV = 1.0f; // W9

	// FolderContents 상태
	std::wstring                      _assetRoot;          // Resources/Assets 절대경로
	std::wstring                      _curDir;             // 현재 탐색 폴더
	std::wstring                      _selectedAsset;      // 선택 파일(인스펙터 표시)

	// 메시/이미지 썸네일 (FolderContents 그리드) — Thumbnail 클래스가 생성/캐시 담당
	Thumbnail                         _thumbnail;
};
