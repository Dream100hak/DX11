## DX11 (Editor/Engine)

이 프로젝트는 **Win32 + DirectX 11** 기반으로 씬/오브젝트를 업데이트하고, 각 오브젝트에 부착된 **Renderer 컴포넌트**를 통해 렌더링을 수행합니다. 모델은 주로 **Assimp로 1회 변환**한 뒤 런타임에서 전용 포맷을 읽어 렌더러에 연결합니다.

## 핵심 연결: DX11 렌더링 루프 ↔ 모델 로드

### 1) 엔트리포인트 → 게임/에디터 루프 진입

- `EditorTool/Main.cpp`의 `WinMain()`에서 `GAME->Run(gameDesc, sceneDesc)`로 실행이 시작됩니다.

### 2) 프레임 흐름의 중심: Scene 업데이트/렌더 호출

- `Engine/SceneManager.cpp`의 `SceneManager::Update()`가 매 프레임 아래 순서로 씬을 진행합니다.
  - `Scene::Update()`
  - `Scene::LateUpdate()`
  - `Scene::Render()`

즉, **게임 루프(프레임) → `SCENE->Update()` → `Scene::Render()`**로 렌더링이 프레임 루프에 연결됩니다.

### 3) 씬/오브젝트 → Renderer 컴포넌트 구조

- 씬은 `GameObject`들의 집합(`Engine/Scene.h`)이며, 각 `GameObject`는 여러 `Component`를 가질 수 있습니다(`Engine/GameObject.cpp`).
- 렌더링은 `ComponentType::Renderer` 슬롯에 들어가는 렌더러들이 담당합니다.
  - **정적 메시**: `MeshRenderer` (`Libraries/Include/Engine/MeshRenderer.h`)
  - **모델(메시/본/머티리얼 집합)**: `ModelRenderer` (`Engine/ModelRenderer.h`)
  - **애니메이션 모델**: `ModelAnimator` (에디터 프리뷰에서 사용)

### 4) 모델 로드가 렌더링으로 “연결”되는 지점

런타임에서 모델 리소스는 `Model`로 표현됩니다(`Engine/Model.h`).

- `Model::ReadModel(...)`: 메시/본/버텍스 등 모델 데이터 로드
- `Model::ReadMaterialByXml(...)` 또는 `ReadMaterial(...)`: 머티리얼/텍스처 로드
- `Model::ReadAnimation(...)`: 클립(애니메이션) 로드

**모델이 실제로 그려지려면** 다음 연결이 필요합니다.

- `GameObject`에 `ModelRenderer`(또는 `ModelAnimator`)를 추가
- `ModelRenderer::SetModel(model)`로 `Model`을 주입
- 이후 씬 렌더 단계에서 해당 렌더러의 `Render(...)`가 호출되며 GPU 바인딩/드로우가 수행됨

이 흐름은 에디터에서 특히 명확히 확인됩니다.

- **프로젝트 창(리소스 스캔/캐싱)**: `EditorTool/Project.cpp`
  - 메시 파일 메타 처리 시 `ReadModel` + `ReadMaterialByXml`로 모델 리소스를 만들고 리소스 캐시에 등록
  - 클립 파일 메타 처리 시 `ReadAnimation`으로 애니메이션을 추가 로드
- **인스펙터 프리뷰(즉시 렌더 연결)**: `EditorTool/Inspector.cpp`
  - `CreateMeshPreviewObj()`에서 `ModelRenderer`를 만들고 `SetModel(model)` 후 썸네일 렌더 큐에 전달
  - `CreateAniPreviewObj()`에서 `ModelAnimator` + `SetModel(model)` 후 프레임/트윈 데이터를 업데이트하며 프리뷰 렌더

### 5) 원본 에셋(FBX/OBJ) → 전용 런타임 포맷(변환 파이프라인)

이 프로젝트는 원본 모델을 직접 런타임에서 파싱하기보다, **Assimp로 한 번 변환해 전용 포맷으로 저장**하는 흐름을 갖습니다.

- 변환기: `EditorTool/AsConverter.cpp/.h`
  - `ReadAssetFile(...)`에서 Assimp로 FBX 등을 로드
  - `ExportModelData(...)`로 **`.mesh`** 저장 (본/메시/버텍스/인덱스/AABB 등)
  - `ExportMaterialDataByXml(...)` 또는 `ExportMaterialDataByMats(...)`로 머티리얼/텍스처 저장
    - `.mat`(개별 머티리얼), `.mmat`(모델이 참조하는 머티리얼 목록)
  - `ExportAnimationData(...)`로 **`.clip`** 저장

런타임의 `Model::ReadModel/ReadMaterial*/ReadAnimation`은 이 산출물들을 읽어 `Model` 내부 데이터를 채우는 형태로 사용됩니다(에디터 측 사용 패턴 기준).

### 6) GPU 업로드: “로드된 모델이 그려질 수 있게 되는 순간”

- 모델 메시 단위로 버퍼 생성이 수행됩니다.
  - `Engine/ModelMesh.cpp`의 `ModelMesh::CreateBuffers()`에서
    - `VertexBuffer::Create(...)`
    - `IndexBuffer::Create(...)`
    를 호출해 GPU 리소스를 준비합니다.

또한 프레임 렌더 단계에서 카메라/월드/라이트/머티리얼/본/키프레임 등 공통 데이터는 `RenderManager`를 통해 상수버퍼로 전달됩니다(`Libraries/Include/Engine/RenderManager.h` 참고).

