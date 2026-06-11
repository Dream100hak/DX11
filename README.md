# DX11 Engine & EditorTool ? 프로젝트 구조 요약 (Project Summary)

> 작성 목적: 향후 질문 및 작업 인수인계를 위한 참고 문서  
> 기준 브랜치: `main`  
> 저장소: https://github.com/Dream100hak/DX11

---

## 1. 솔루션 구성

| 프로젝트 | 경로 | 역할 |
|----------|------|------|
| **Engine** | `Engine/Engine.vcxproj` | 공용 런타임 엔진 라이브러리 (렌더링·씬·리소스 등) |
| **EditorTool** | `EditorTool/EditorTool.vcxproj` | DX11 에디터 앱 (ImGui 기반 IDE-형 도구) |

---

## 2. 전체 아키텍처 패턴

### 2-1. Singleton 매니저 패턴
엔진의 핵심 시스템은 전부 `DECLARE_SINGLE` 매크로로 선언된 싱글턴입니다.

```
Game        ? 앱 진입점, 윈도우/메시지루프
Graphics          ? D3D11 Device / DeviceContext
InputManager      ? 키·마우스 입력
TimeManager       ? 델타 타임
ResourceManager   ? 텍스처·메시·셰이더·머티리얼·모델 캐시
SceneManager      ? 씬 전환 및 현재 씬 보유
InstancingManager ? GPU 인스턴싱 배치 관리
ImGuiManager      ? ImGui 초기화·렌더
RenderStateManager? 블렌드/래스터라이저/뎁스스텐실 상태
ProjectManager    ? 에디터 프로젝트 경로 관리
```

**접근 매크로** (`Engine/Define.h`)

```cpp
GAME / GRAPHICS / DEVICE / DCT
INPUT / TIME / DT
RESOURCES / INSTANCING / GUI / SCENE / PROJECT
CUR_SCENE / MAIN_CAM
RENDER_STATES
```

### 2-2. Component 기반 GameObject (Unity-style ECS)

```
GameObject
 ├─ _components[FIXED_COMPONENT_COUNT]  ← 고정 슬롯 (ComponentType enum 인덱스)
 │    Transform / Camera / Renderer / Animator
 │    Light / Collider / Terrain / Button / BillBoard / SkyBox
 └─ _scripts[]  ← MonoBehaviour 파생 스크립트 (동적 추가)
```

- `ComponentType::End` 직전까지가 고정 슬롯 (`FIXED_COMPONENT_COUNT`)
- `GetComponent<T>()` ? 고정 슬롯 + 스크립트 모두 `dynamic_pointer_cast` 검색
- **생명 주기**: `Awake → Start → Update → LateUpdate → FixedUpdate`
- 씬 오브젝트는 항상 `Scene`이 소유 (`unordered_set<shared_ptr<GameObject>>`)

### 2-3. Scene / SceneManager

```
SceneManager (Singleton)
 └─ shared_ptr<Scene>  _currentScene

Scene
 ├─ _objects          (unordered_set) ? 전체 오브젝트
 ├─ _cameras   (unordered_set) ? 카메라 캐시
 ├─ _lights     (unordered_set) ? 라이트 캐시 (멀티 라이트 지원)
 ├─ _terrains         (unordered_set) ? 지형 캐시
 ├─ _createdObjectsById   (map<int64>)  ? 시간순 정렬 캐시
 └─ _createdObjectsByName (map<wstring>) ? 이름 검색 캐시
```

씬 전환: `SceneManager::ChangeScene<T>(shared_ptr<T>)` → 이전 씬 해제 후 `Start()` 호출.

---

## 3. 렌더링 파이프라인

### 3-1. 렌더 흐름 요약

```
Game::Update()
 └─ SceneManager::Update()
      └─ Scene::Update()  →  Scene::Render()
         └─ Camera::SortGameObject()   ← Frustum Culling + RenderQueue 분류
                └─ Camera::Render_Forward()
     ├─ RenderContext 구성 (view / proj / lightArray)
         ├─ InstancingManager::Render(ctx, _vecOpaque)    ← Front-to-Back
           └─ InstancingManager::Render(ctx, _vecTransparent) ← Back-to-Front
```

### 3-2. RenderContext (단일 Draw 진입점)

```cpp
struct RenderContext {
    int32  tech;         // 테크닉 인덱스
    Matrix view, proj;
    shared_ptr<Light>  light;         // 단일 라이트 (레거시 호환)
    shared_ptr<LightArrayDesc> lightArray;    // 멀티 라이트 배열 (최대 16)
    shared_ptr<Shader>    shaderOverride; // FX11 오버라이드
    shared_ptr<HlslShader>     hlslOverride;   // HLSL 오버라이드 (Shadow/Outline 등)
    shared_ptr<InstancingBuffer> buffer;  // nullptr = Single, 값 있으면 Instanced
};
```

`Renderer::Draw(const RenderContext&)` 가 단일 가상 함수 ? `MeshRenderer`, `ModelRenderer`, `ModelAnimator` 가 오버라이드.

### 3-3. RenderQueue (렌더 순서)

| 값 | 큐 이름 | 정렬 | 대표 오브젝트 |
|----|---------|------|--------------|
| 1000 | Background | ? | 스카이박스 |
| 2000 | Opaque | Front-to-Back (Early-Z) | 일반 메시 |
| 2450 | AlphaTest | Front-to-Back | 알파 클립 |
| 3000 | Transparent | Back-to-Front | 파티클 |
| 4000 | Overlay | ? | UI |

`Material`이 `_renderQueue` 값을 보유. `Camera::SortGameObject()`에서 분리.

### 3-4. Frustum Culling

`Camera`가 `Frustum` 멤버 보유.  
`Frustum::Update(viewProj)` → 6평면 추출, `IsInFrustum(AABB)` p-vertex 방식 판정.  
바운딩박스 미계산 오브젝트는 자동 통과.

### 3-5. GPU 인스턴싱

`InstancingManager::Render()` → 동일 `InstanceID`(Mesh+Material 해시)끼리 배치.  
- 정적 오브젝트: `RenderStaticObject()` (private)  
- 애니메이션: `RenderAnimRenderer()` (private)  
공개 API는 `Render()` 하나만 노출.

---

## 4. 셰이더 시스템

### 4-1. 이중 셰이더 구조 (마이그레이션 과도기)

| 시스템 | 파일 | 상태 |
|--------|------|------|
| **FX11 (레거시)** | `Shader.h` + `.fx` 파일 | Terrain/SSAO 등 일부 잔존 |
| **HlslShader (신규)** | `HlslShader.h/.cpp` + `.hlsl` | 주요 셰이더 전환 완료 |

`Material`이 두 포인터(`_shader`, `_hlslShader`)를 모두 보유 ? 렌더 시 `hlslOverride > hlslShader > fxShader` 우선순위.

### 4-2. 실제 사용 셰이더 목록

**Engine 로드:**
- `Standard.fx/.hlsl` ? 메인 Forward 렌더링
- `ShadowMap.fx/.hlsl` ? 섀도우 맵
- `Ssao.fx` ? SSAO (FX 유지)
- `Sky.fx/.hlsl` ? 스카이박스
- `Outline.fx/.hlsl` ? 오브젝트 아웃라인
- `Terrain.fx` ? 지형 (HS/DS 미지원으로 FX 유지)
- `Thumbnail.fx/.hlsl` ? 메시 프리뷰 썸네일
- `Collider.fx` ? 콜라이더 디버그

**EditorTool 로드:**
- `CubeMap.fx` / `SceneGrid.fx` / `DebugTexture.fx`
- `Fire.fx` / `Rain.fx` ? 파티클

### 4-3. HlslShader 상수 버퍼 슬롯 (Common.hlsli 기준)

| 슬롯 | 내용 |
|------|------|
| b0 | GlobalBuffer (View / Proj / 카메라 위치) |
| b1 | TransformBuffer (World 행렬) |
| b2 | LightBuffer (단일 라이트) |
| b3 | MaterialBuffer (색상·계수) |
| b4 | BoneBuffer (스키닝 팔레트) |
| b5 | KeyframeBuffer (애니메이션 키프레임) |
| b6 | TweenBuffer (인스턴스 블렌딩) |
| b7 | LightArrayBuffer (MAX_LIGHTS=16 배열) |

---

## 5. 리소스 관리

```
ResourceManager (Singleton)
 └─ _resources[RESOURCE_TYPE_COUNT]
      각 타입별 map<wstring, shared_ptr<ResourceBase>>
      (Texture / Mesh / Material / Shader / Model)
```

- `Load<T>(key, path)` ? 캐시 히트 시 반환, 미스 시 `Load(path)` 호출 후 캐시 저장
- `GetOrAddTexture()` / `GetOrAddHlslShader()` ? 특수 오버로드
- `ResourceBase` ? 공통 기반 클래스 (`Load(path)` 순수 가상)

---

## 6. 멀티 라이트 지원 (Phase 1)

```cpp
// Define.h
#define MAX_LIGHTS 16

// BindShaderDesc.h
struct DirectionalLightData { ... };
struct LightArrayDesc { DirectionalLightData lights[MAX_LIGHTS]; int count; };

// RenderContext.lightArray 로 전달
// Camera::Render_Forward() 에서 Scene::GetLights() 로 수집
// MeshRenderer::Draw() 에서 lightArray 우선 처리
```

---

## 7. EditorTool 구조

### 7-1. 전체 구조

```
EditorTool (IExecute 구현 ? 앱 진입점)
 └─ EditorToolManager (Singleton)
    ├─ _editorWindows (unordered_map<string, EditorWindow>)
      ├─ _selectedH  (int64 ? Hierarchy 선택 오브젝트 ID)
 ├─ _selectedP  (shared_ptr<MetaData> ? Project 선택 에셋)
      ├─ _selectedFolder / _selectedItem
    └─ _cashesFileList / _cashesModelList (에셋 캐시)
```

### 7-2. EditorWindow 파생 클래스

| 클래스 | 역할 |
|--------|------|
| `Hiearchy` | 씬 오브젝트 계층 목록 (현재 평면 리스트) |
| `Inspector` | 선택 오브젝트 컴포넌트 편집 |
| `SceneWindow` | 씬 뷰 (RenderTexture 출력) |
| `GameEditorWindow` | 게임 뷰 |
| `Project` | 프로젝트 폴더 트리 |
| `FolderContents` | 폴더 내 에셋 아이콘 목록 |
| `LogWindow` | 로그 출력 창 |
| `MainMenuBar` | 메뉴바 (파일 저장·불러오기 등) |

`EditorToolManager::GetEditorWindow(name)` ? 못 찾으면 `static nullptr` 반환 (UB 방지).

### 7-3. 에디터 전용 매크로 (`EditorTool/Define.h`)

```cpp
#define TOOL     EditorToolManager::GetInstance()
#define SELECTED_H    TOOL->GetSelectedIdH()
#define SELECTED_P    TOOL->GetSelectedIdP()
```

### 7-4. ShortcutManager

키보드 단축키 처리 전담 ? `DeleteObject()` 등 씬 조작 기능 포함.

---

## 8. 주요 컴포넌트 상호작용 다이어그램

```
[Game::Run()]
    │
    ├─ [InputManager::Update()]
    ├─ [TimeManager::Update()]
    │
    ├─ [SceneManager::Update()]
    │       └─ [Scene::Update()] → 각 GameObject::Update()
    │  └─ Component::Update() (Transform, Camera, Light ...)
    │       └─ MonoBehaviour::Update() (사용자 스크립트)
    │
    ├─ [Scene::Render()]
    │       └─ Camera::SortGameObject()   ← Frustum + RenderQueue 분류
  │        └─ Camera::Render_Forward()
    │  ├─ RenderContext 구성
    │       ├─ InstancingManager::Render(_vecOpaque)
    │                │└─ MeshRenderer/ModelRenderer/ModelAnimator::Draw(ctx)
    │         │└─ HlslShader::Bind() → DrawIndexed/DrawIndexedInstanced
    │        └─ InstancingManager::Render(_vecTransparent)
    │
  └─ [ImGuiManager::Render()]  ← EditorTool 전용
            └─ EditorToolManager::Update()
      └─ 각 EditorWindow::Update()
   ├─ Hiearchy / Inspector / SceneWindow ...
                 └─ RenderTexture → SceneWindow 표시
```

---

## 9. 현재 진행 상태 요약 (`ENGINE_IMPROVEMENT_REPORT.md` 기준)

| 단계 | 내용 | 상태 |
|------|------|------|
| Step 1~6 | FX11 → HlslShader 마이그레이션 | ? 완료 |
| Step 7 | Frustum Culling | ? 완료 |
| Step 8 | Render Queue (Opaque/Transparent 분리) | ? 완료 |
| Step 9 | RenderContext 단일 Draw 진입점 | ? 완료 |
| Step 10 | InstancingManager 시그니처 정리 | ? 완료 |
| Step 11 | UpdateTweenData() ModelAnimator::Update()로 이동 | ? 완료 |
| Step 12 | RenderContext hlslOverride 지원 | ? 완료 |
| Step 13 | 멀티 라이트 지원 Phase 1 | ? 완료 |
| **Step 14** | **Deferred Rendering (Phase 2)** | ? 다음 작업 |

---

## 10. 알려진 개선 필요 사항 (`IMPROVEMENT_REPORT.md` 기준)

### 즉시 수정 (버그)
- `Hiearchy.cpp` ? `Selectable(name, (isSelected, flags))` 쉼표 연산자 버그 → `isSelected` 분리 필요
- `wstring→string` 변환 ? 한글 깨짐 → `Utils::ToString()` 통일 필요

### 단기 (코드 품질)
- `ImGui::Begin()` 반환값 미처리 및 `End()` 누락 패턴
- `ImGuiIO& io` 미사용 변수 (`Hiearchy.cpp`)
- 오타: `Hiearchy` → `Hierarchy`, `_cashesFileList` → `_cachedFileList`
- `EditorToolManager::GetMetaType()` ? `if-else if` 체인 → `unordered_map` 룩업 테이블

### 중기 (구조)
- `EditorToolManager` 책임 분리 (`SelectionContext`, `AssetCache`)
- 썸네일 프리뷰 카메라/라이트 공유 (`PreviewSceneContext`)
- `_cashesFileList` `map` → `unordered_map`

### 장기 (기능)
- Undo/Redo Command 패턴
- Hierarchy TreeNode 계층 구조
- 다중 오브젝트 선택 (`set<int64>`)
- "Add Component" UI

---

*이 문서는 코드 분석 기반으로 자동 생성되었습니다. 구조 변경 시 함께 업데이트하세요.*

---

## 11. 엔진 구조적 개선 필요 사항

> 에디터 UI 문제(섹션 10)와 별개로, **Engine 프로젝트 내부**의 설계·구현 문제를 정리합니다.

---

### 11-1. `Graphics` ? 단일 책임 원칙 위반

`Graphics`가 아래 역할을 동시에 담당합니다.

- D3D11 Device / SwapChain / RTV / DSV 초기화
- **JobQueue 3개 직접 보유** (`PreRender` / `Render` / `PostRender`)
- `DepthStencilState` 2종 직접 보유 (`Standard` / `Outline`)
- `RasterizerState` (Wireframe) 직접 보유

`RenderStateManager`가 이미 존재하므로, DS/RS 상태 관리는 `RenderStateManager`로 이전하는 것이 일관성 있습니다.  
JobQueue 역시 별도 `RenderScheduler` 또는 `FrameJobManager` 클래스로 분리를 권장합니다.

```
권장 분리:
├── Graphics          ← Device / SwapChain / RTV / DSV / Viewport만
├── RenderStateManager ← 모든 BS / RS / DSS 통합 (현재 일부만 담당)
└── RenderScheduler   ← PreRender / Render / PostRender JobQueue
```

---

### 11-2. `Light` ? `LightType` 열거형 선언만 존재, Point/Spot 미구현

`Light.h`에 `Point`, `Spot` 타입이 열거되어 있고 `BindShaderDesc.h`에 `PointLightDesc` / `SpotLightDesc` 구조체도 존재하지만, 실제 렌더링 경로(`LightArrayDesc`)에는 **Directional Light만 사용**됩니다.

- `LightArrayDesc::lights[]`는 `DirectionalLightData` 배열로 고정
- `PointLightDesc` / `SpotLightDesc`는 정의만 있고 CB 슬롯·셰이더 연동이 없음
- Inspector UI의 `Combo("LightMode", ...)` 조건문에 세미콜론 버그 존재

```cpp
// Light.h ? 버그: if(...); 로 조건문이 즉시 종료되어 _type이 절대 바뀌지 않음
if (ImGui::Combo("LightMode", &selected, "Directional\0Point\0Spot\0"));
// ↑ 세미콜론 제거 필요
{
    _type = static_cast<LightType>(selected);
}
```

**권장**: Point/Spot을 지원할 계획이 없다면 열거형과 Inspector UI를 Directional 전용으로 단순화.  
지원할 계획이면 `LightArrayDesc`를 타입별 배열로 확장하고 셰이더와 함께 구현.

---

### 11-3. `BVHNode` ? 헤더에 렌더 로직 전체 구현

`BVH.h`에 `BVHNode::Start()` / `BVHNode::Update()` 가 헤더 내부에 완전히 구현되어 있으며, `Update()` 내부에서 직접 `SCENE->GetCurrentScene()->GetMainCamera()` 를 매 프레임 호출합니다.

```cpp
// BVH.h ? 문제점 요약
// 1. 렌더 관련 멤버(geometry, vertexBuffer, material 등)가 BVHNode(데이터 구조)에 혼재
// 2. Update()가 매 프레임 씬 전역 조회 → 렌더 데이터와 가속 구조가 결합
// 3. 헤더에 구현된 복잡한 로직
