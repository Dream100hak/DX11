# CLAUDE.md - DX11 Engine Project

## Project Overview

DirectX 11 based game engine + editor project (for learning / portfolio).
Built with Visual Studio solution `Pot.sln`, consisting of Engine (static library) and EditorTool (executable).

- **Repository**: https://github.com/Dream100hak/DX11
- **Branch**: main
- **Language**: C++20, HLSL 5.0
- **Build**: Visual Studio (MSBuild), x64

## File Encoding (CRITICAL)

- **ALL files MUST be saved as UTF-8 (without BOM)** - non-UTF-8 encoding will break Korean text
- This applies to C++/H sources, HLSL shaders, MD documents, config files - NO exceptions
- Both vcxproj compile with **`/utf-8`** flag (commit 78-2) — BOM-less UTF-8 sources compile correctly
- `.editorconfig` at repo root enforces charset for VS2022/VS Code
- Always verify UTF-8 encoding when creating new files

## Project Structure

```
DX11/
+-- Engine/                 # Core engine (static library)
|   +-- Graphics.h/.cpp     # D3D11 device/swapchain
|   +-- HlslShader.h/.cpp   # Native HLSL shader wrapper (VS/PS/HS/DS/GS/CS, Stream-Output, cbuffer Push)
|   +-- Material.h/.cpp     # Material system (HLSL only)
|   +-- MeshRenderer/ModelRenderer/ModelAnimator  # Draw(RenderContext) single entry
|   +-- Camera.h/.cpp       # Camera + Render_Deferred() pipeline
|   +-- Frustum, InstancingManager, RenderContext, Scene, GameObject, Transform, ...
|   +-- ImGuizmo.h/.cpp     # Gizmo library (next to imgui files)
|   +-- ModelMesh.h         # MeshAabb (.mesh format AABB, replaced aiAABB)
+-- EditorTool/             # Editor application
|   +-- EditorTool.*        # Entry point
|   +-- EditorToolManager.* # Window management (unordered_map<string, EditorWindow>)
|   +-- Inspector.* + InspectorHierarchy.cpp + InspectorProject.cpp  # Property inspector (split)
|   +-- Hiearchy.*          # Scene hierarchy (typo: should be Hierarchy)
|   +-- SceneWindow.*       # Scene view: ImGuizmo gizmo + click picking + drag-drop placement
|   +-- UfbxConverter.*     # FBX -> .mesh/.clip/.mat/.mmat converter (ufbx based)
|   +-- FolderContents.* / Project.* / MainMenuBar.* / LogWindow.*
+-- Shaders/HLSL/           # All shaders (Common.hlsli, Lighting.hlsli, Standard_*, GBuffer_PS,
|                           #  DeferredLighting, ShadowMap, Ssao*, Terrain, Fire/Rain, PostProcess,
|                           #  Fxaa, IblBake, PassViewer, Outline, Sky, ...)
+-- Libraries/
|   +-- Include/            # External headers: DirectXTex, magic_enum, ufbx
|   +-- Lib/                # External binaries: DirectXTex (Engine.lib output is git-ignored)
+-- Resources/
|   +-- PrevConverted/      # Source FBX files (Kachujin/Archer/Mutant/Tower)
|   +-- Assets/Models/      # Converted .mesh/.clip/.mat/.mmat + textures
+-- Pot.sln
```

## Architecture & Rendering Pipeline

### Shader System
- **FX11 FULLY REMOVED**, **Assimp FULLY REMOVED** — zero .fx files, no external importer lib.
- `HlslShader`: VS/PS/HS/DS/GS/CS compile, auto InputLayout, cbuffer Push methods, Stream-Output GS + DrawAuto (particles).
- GOTCHA: `HlslShader::Draw/DrawIndexed` force TRIANGLELIST internally — particle SO kick-off must use raw `DCT->Draw(1,0)` (POINTLIST).

### Rendering Flow (Deferred PBR + HDR)
```
Camera::Render_Deferred()
  Pass 1:   GBuffer fill (albedo+metallic / normal+roughness / worldpos+mask / emissive) + Terrain GBuffer + Foliage(잔디/나무 인스턴싱)
  Pass 2:   DeferredLighting fullscreen -> HDR sceneColor (Cook-Torrance + IBL t5/t6/t7 + CSM Shadow t3 + SSAO t4 + spot t9 / point cube t10)
            + Clustered shading: 디렉셔널은 b7 인라인(CSM), 점/스팟은 froxel 클러스터 라이트 리스트(t11~t13)로 픽셀당 자기 클러스터만 순회
  Pass 2.5: SSR (Ssr.hlsl) — sceneColor+GBuffer 월드 레이마치 반사 -> _ssrTex -> CopyResource sceneColor
  Pass 2.7: Volumetric (Volumetric.hlsl) — CSM 그림자 레이마치 갓레이(HG phase) -> _volTex -> CopyResource sceneColor
  Pass 3:   Background(sky)/Transparent(particles)/Overlay -> sceneColor (GBuffer depth shared)
  Bloom:    BrightPass -> BlurH -> BlurV (b8 PostProcessBuffer)
  Exposure: Exposure.hlsl 로그휘도 -> 256² 밉체인(GenerateMips) 평균
  Pass 4:   Tonemap(ACES + Auto-exposure + gamma, bloom t1) -> FXAA -> backbuffer
  + PassViewer overlay (scene top-left combo / KEY_4), KEY_3 GBuffer split debug
```
- IBL: `Engine/Ibl` bakes once at startup (`IblBake.hlsl` — irradiance/prefiltered/BRDF LUT). Env map = desertcube1024.dds (shared with skybox).
- GOTCHA: SSR/Volumetric 은 별도 RT 에 합성 후 `CopyResource` 로 sceneColor 복원 + **렌더타겟을 sceneColorRTV+GBuffer DSV 로 복원**해야 Pass 3 스카이박스가 sceneColor 에 그려짐 (안 하면 스카이 검게).
- Post-process toggles/params: Camera inspector — Bloom(Threshold/Intensity), FXAA, Auto Exposure(Key), SSR, Volumetric(Density/Intensity/Scatter G), EnvIntensity, Cascade Debug, Wire.

### Constant Buffer Layout
- `b0` GlobalData (V,P,VP,VInv,Shadow,Time) / `b1` TransformData / `b2` LightData / `b3` MaterialData (+Roughness/Metallic)
- `b4` BoneData / `b5` ModelBoneBuffer (per-mesh static bone) / `b6` TweenBuffer / `b7` LightArray (포워드 MAX_LIGHTS=16) — **DeferredLighting 에선 b7 = ClusterParams**
- `b8` pass-specific (Terrain/Ssao/Particle/IblBake/Ibl/PostProcess/PassViewer)
- Sampler s0~s4, Material SRV t0~t4 (Diffuse/Specular/Normal/Shadow/Ssao)

### Clustered Shading (commit 165)
- **`Engine/ClusterLighting`** — 뷰 절두체를 16×9×24 froxel 격자(Z 로그 분포)로 분할, **CPU 라이트 컬링**.
  매 프레임 씬의 모든 점/스팟 라이트를 자기가 겹치는 클러스터에만 배정(구-AABB 테스트) → 클러스터별 라이트 인덱스 리스트를
  동적 구조화버퍼(`StructuredBuffer` SRV 경로 재사용)로 업로드. **MAX_LIGHTS 16 제약 제거(수백 개)**.
- GPU 바인딩: `t11` ClusterLights(LightData[256]) / `t12` ClusterCounts(uint[3456]) / `t13` ClusterIndices(uint[3456×64]) / `b7` ClusterParams.
  DeferredLighting PS 는 픽셀 uv+viewZ 로 클러스터 인덱스를 산출해 해당 클러스터 라이트만 순회 (셰이더·CPU 슬라이스 매핑 동일해야 함).
- **디렉셔널 라이트는 클러스터 컬링 대상 아님** — 화면 전체 영향이라 ClusterParams(b7) 에 인라인(`DirLights[4]`), CSM 그림자 유지.
- 포워드(투명/파티클) 경로는 여전히 CollectLights 의 16개 cbuffer LightArray(b7) 사용 — 셰이더가 달라 b7 레이아웃 충돌 없음.
- 한계: CPU 컬링(추후 컴퓨트셰이더 이관 가능), 포워드 경로 16개 캡 잔존.

### Render Queue
- `RenderQueue`: Background / Opaque / AlphaTest / Transparent / Overlay (set via Material `_renderQueue`)
- `RenderContext` flags: `deferredPass` / `shadowPass` / `ssaoPass` select shader per pass in each renderer.

## Model Pipeline (ufbx, commits 85~89)

- **Converter**: `EditorTool/UfbxConverter` (ufbx single-file lib in `Libraries/Include/ufbx`).
  Output format byte-compatible with legacy AsConverter: `.mesh` (bones+meshes+VertexTextureNormalTangentBlendData+MeshAabb),
  `.clip` (baked per-frame keyframes via `ufbx_evaluate_transform`), `.mat`/`.mmat` (materials incl. PBR roughness/metallic).
- Improvements over old Assimp path: real tangents (FBX tangents or UV-based generation — old path wrote zeros),
  PBR extraction (old path hardcoded 0.5/0.0), embedded texture extraction byte-identical.
- Conversion options: left-handed Y-up, mirror Z (winding auto-flipped), `target_unit_meters=1`,
  `SPACE_CONVERSION_MODIFY_GEOMETRY` (vertices in meters — matches legacy output scale), UV V-flip (1-v).
- Skin bone mapping by **node pointer** (not name — duplicate/empty names crash).
- **Editor trigger**: File > Convert FBX... (file dialog; mesh -> `Models/<parent folder>/`, clips -> `<stem>.clip`).
- Engine `.mesh` AABB type = `MeshAabb` (ModelMesh.h, same 24-byte layout as old aiAABB — legacy assets compatible).
- `.mmat` materials: `Model::GetMaterialByName` matches by filename stem too (full-path names from Material::Load) —
  without this mesh->material is null and models sample leaked SRVs (green/rainbow corruption).
- Project/SceneWindow load `.mmat` first, legacy `.xml` fallback.

## Editor (commits 79~92)

- **Scene view gizmo = ImGuizmo** (Engine/ImGuizmo.*, `ImGuizmo::BeginFrame()` in ImGuiManager::Update).
  Move/Rotate/Scale + Local/World + snap (0.5 / 15deg / 0.1). W/E/R shortcuts (ignored while RMB camera fly),
  F = focus selected. Toolbar radio buttons in scene view. Global `OPERATION` enum (Game.h) is bit-compatible
  with `ImGuizmo::OPERATION` (cast directly). Old hand-ported gizmo fully deleted (commit 92).
- **Click picking**: `Scene::MeshPick` -> `Renderer::Pick` (AABB pre-test + triangle raycast). ModelAnimator (commit 133):
  현재 애니 포즈 기준 — TransformMap 과 동일한 `_animTransforms` 본 행렬(curr/next 프레임 보간)을 CPU 스키닝으로 적용,
  AABB 는 포즈 변형 여유로 1.6배 확장. 애니 데이터 없으면 바인드포즈 폴백. Picking gated by
  `_bUsing = ImGuizmo::IsUsing() || IsOver()` + `GUI->IsHoveringWindow()`.
- **Inspector**: split into InspectorHierarchy.cpp / InspectorProject.cpp. Component names via
  `ImGuiManager::EnumToString` (static switch for CreatedObjType/ComponentType/RendererType — no enum lib).
  Renderer slot shows concrete type (MeshRenderer/ModelAnimator/...). Component Delete works
  (`GameObject::RemoveComponent`; Transform refused, main-camera Camera refused).
- **ParticleSystem inspector**: Emit Dir/Accel/Emit Interval/Lifetime/Initial Speed/Size editable
  (ParticleBuffer b8 — bound to **both GS and VS**; VS-only fields read 0 otherwise = frozen particles, commit 84).
- **Particle frustum culling** (commit 132): `ParticleSystem::TransformBoundingBox` 오버라이드 — Rain 은 카메라 중심
  분산반경 박스, Fire 는 트랜스폼 중심 방출범위 박스. 기본(오브젝트 위치 1m 박스)이면 카메라가 오브젝트 지점을
  안 볼 때 시스템 전체(SO 시뮬레이션 포함)가 컬링됨 — "Rain 안 보임" 버그의 원인이었음.
- Light: zero direction falls back to (0,-1,0) (XMMatrixLookAtLH assert crash on Add Component, commit 81).
- Shadow bounds: fixed center/radius on Light inspector — objects outside cast/receive no shadows (auto-fit reverted by user preference).

## Hierarchy Parenting (commits 116~119)

- `Transform::SetParentKeepWorld` — world-preserving reparent (null = to root), refuses self/descendant (cycle guard).
  Parent ref is **weak_ptr** (shared both ways would leak). `Scene::Remove` detaches from parent and promotes children to root.
- Hierarchy window renders a recursive TreeNode tree (roots only at top level). **Drag-drop**: onto a node = parent (world kept),
  onto empty space = unparent. `.scene` stores `id`/`parent` attrs; load is 2-pass (create all → link, LOCAL-preserving — not KeepWorld).
- Gizmo write-back uses `Transform::SetWorldMatrix` (world→local inverse vs parent) — per-channel SetPosition/Rotation/Scale
  decomposition breaks under a rotated/scaled parent.

## Scene Save/Load & Play Mode (commits 101~104)

- **`.scene` XML** (`EditorTool/SceneSerializer`): Transform / MeshRenderer / ModelRenderer / ModelAnimator(+clips) /
  Light / Camera / Terrain / SkyCubeMap / ParticleSystem. File > New/Open/Save Scene (default `Resources/Assets/Scenes/`).
- Materials: `.mat`-backed → `MaterialRef` path (shared cache), clones → inline MaterialDesc + texture paths.
- `GameObject::SetEditorInternal` excludes editor infra (editor camera, folder previews) from serialization.
- **Play/Stop**: snapshot on Play (`__play_snapshot.scene`), restore on Stop — edits during play auto-rollback (Unity semantics).
- **Game view**: while playing, renders the first non-internal Camera ("GameObject > Create Camera") over the scene view.
  Editing mode shows a **camera preview inset** (scene view bottom-right) when a game camera is selected (closable, x).
  Both use the FULL deferred pipeline — `Camera::Render_Deferred` renders from `this` camera and
  `SetFinalOutput(rtv)` overrides the final target (null = backbuffer).
- `Scene::GetMainCamera` prefers the editorInternal camera — placed game cameras can't hijack the editor viewpoint.

## Material System (commits 98~100)

- Cache key = `Utils::ToMaterialKey` (canonical lowercase path) — same `.mat` everywhere is ONE instance:
  inspector edits hit scene models live; "Save Material" button persists (`Material::Save`, mirror of Load).
- Diffuse color is a tint (multiplied with texture) in GBuffer/forward/preview. Ambient/Specular = forward-only legacy.
- **Emissive**: GBuffer RT3 (R11G11B10F), `MatEmissive.rgb × a` (alpha = intensity); HDR values bloom.
- Material preview sphere uses `MeshPreview_HLSL` (PS_PreviewLit) — scene forward PS would render black (unbound shadow map).

## DockSpace & Scene-to-RT (commit 124)

- ImGui = **v1.89.7-docking branch** (docking only exists there). `DockingEnable` + `DockSpaceOverViewport`,
  default layout via DockBuilder when no imgui.ini (ImGuiManager::BuildDefaultDockLayout).
- **Scene view = RT image** (backbuffer passthrough abandoned — incompatible with docking): editor camera renders
  deferred into SceneWindow's RT via `Camera::SetFinalOutput`; `Scene::Render` restores backbuffer after (ImGui draws there).
- SceneDesc = scene IMAGE rect (updated in SceneWindow::DrawSceneImage) — picking/gizmo/overlays/viewport all follow it.
  RT recreated on window resize. "Game" docks as a tab next to "Scene" (focused on Play).
- imgui.ini is a runtime artifact (gitignored).

## Thumbnails & Model Inspector (commits 126~128)

- **SSAO stale SRV**: `Ssao::Resize` recreates `_ambientSRV0` — holders of the old SRV (DefaultMaterial,
  bound by deferred lighting + PassViewer) freeze on first-frame AO ("SSAO doesn't follow camera").
  `Ssao::Draw` re-sets the material's SSAO map every draw. Game view uses its **own Ssao instance**
  (sharing caused per-frame editor-size/game-size Resize ping-pong = texture recreation every frame).
- **MeshThumbnail**: origin viewport (SceneDesc offset used to shift content off-center) +
  `ComputeFitViewProj` — world-AABB circumscribed-sphere auto-fit, 3/4 view, square aspect (all 4 call sites).
- **MESH inspector** (`Inspector::DrawModelDetails`): Model Info summary, per-mesh table (verts/tris/material),
  Skeleton bone TreeNode hierarchy (ModelBone children cache from BindCacheInfo), Materials/Clips lists.
  Clip inspector shows frames/fps/duration.

## Editor Overlays (commit 130)

- **Selection outline**: `Camera::RenderOutlinePass` (Pass 3 직후, editorInternal 카메라 전용 게이트) — 스텐실 2패스
  (OutlineMark: 메시 영역 마크/색·깊이 기록 없음 + NoColorWrite 블렌드, OutlineDraw: 노멀 팽창 메시를 NOT_EQUAL 영역만).
  `Outline_HLSL`/`OutlineModel_HLSL`/`OutlineAnim_HLSL` (Outline_VS.hlsl 진입점 3종, OutlineBuffer는 **b8** — b7은 LightArray와 충돌).
  단일 드로우 전용(b1 W 사용, 인스턴싱 시맨틱 없음). 폭은 카메라 거리 비례(화면상 두께 일정). `_isOutlined` + `GetUIPicked()` 대상.
- **Scene grid**: `EditorTool/SceneGrid` — editorInternal 씬 오브젝트 2개(1m/10m 셀), `RendererType::Grid` +
  `Renderer::_renderQueue=Transparent`로 Pass 3 렌더 (deferred/shadow/ssao 패스 가드). 카메라 XZ 셀 스냅 추적으로 무한 그리드처럼 보임.
  b8 GridParamsBuffer(페이드 거리/알파), X축 빨강·Z축 파랑. HDR에 블렌드 후 ACES를 거치므로 라인은 어둡게+알파 높게 해야 보인다.
- GOTCHA: 커스텀 Renderer에 `RendererType::Mesh`를 쓰면 `GetMeshRenderer()` 타입 체크를 통과해 **잘못 캐스팅(UB)** — 전용 enum 필수.
  베이스 `Renderer::TransformBoundingBox`는 1×1×1 고정 박스(대형 커스텀 렌더러는 virtual 오버라이드 없으면 절두체 컬링당함),
  베이스 `GetInstanceID()`는 (0,0) 고정(오버라이드 없으면 같은 타입끼리 배칭돼 하나만 그려짐).
- `Camera::SortGameObject`: editorInternal 오브젝트는 editorInternal 카메라(씬 뷰)에서만 렌더 — Game 뷰/프리뷰에 그리드·드래그 프리뷰 안 보임.

## Terrain Editor & Foliage (commits 144~153)

- **TerrainWindow** (도킹 창): Sculpt(Raise/Lower/Smooth/Flatten) + Paint(레이어 블렌드) + Save/Load + Foliage(잔디/나무).
- **Sculpt**: 브러시 반경 내 `_heightmap`(CPU float) 수정 → 패치 Y바운드 재계산 → GPU 높이맵 텍스처 `UpdateSubresource`.
  정점 재생성 없이 DS 변위/노멀/CPU `GetHeight` 동기화. `Terrain::RaycastTerrain`(높이필드 march+이분탐색)로 브러시 중심.
- **Paint**: 기존 `blend.dds`(비압축 BGRA, 밉맵 다수)를 스테이징 **mip0 CopySubresourceRegion** 으로 리드백 → CPU 미러+편집 텍스처 승격(`_blendMap` SRV 교체). 채널 R/G/B/A=레이어1~4, base=레이어0.
- **Save/Load**: 높이맵=`<stem>_edit.r32`(float32, 8-bit 계단현상 없음), 블렌드=`<stem>_edit.dds`. TerrainInfo 경로 갱신 → .scene 영속. `.r32` 로드 시 Smooth 생략.
- **Create Terrain**(Hiearchy): GameObject 생성 → Terrain 부착+Init → **마지막에 Scene::Add**(그래야 `_terrains` 등록 — CreateLight 패턴). CreateEmptyGameObject(빈 obj 선등록) 쓰면 GetTerrain null.
- **Foliage** (`Engine/Foliage`, Grass/Tree): Terrain 소유, Camera Pass 1 에서 터레인 직후 렌더. 자체 인스턴스 버퍼(수천~, InstancingManager 캡 500 우회), 16×16 청크 + Frustum/거리 컬링, VS 거리 페이드. Grass=절차적 블레이드 알파컷, Tree=절차적 저폴리(줄기+원뿔). 블렌드맵 레이어 가중치 비례 밀도(거부 샘플링). 생성 파라미터만 .scene 저장 → 로드 시 결정적 재생성.

## Known Issues

- `Hiearchy` filename typo kept for compatibility.

## Build & Run

1. Open `Pot.sln`, x64 Debug/Release, EditorTool as startup project.
2. EditorTool references Engine via **ProjectReference**; includes Engine headers directly from `Engine\`
   (no header xcopy — that caused stale-pch bugs). `Libraries\Include\` = external headers only.
3. `ufbx.c` and `ImGuizmo.cpp` compile with `PrecompiledHeader=NotUsing`.

## Workflow Conventions

- Numbered Korean commits: `NN. 제목` (`git commit -F <file>` — PowerShell `-m` corrupts Korean). Push when an arc completes.
- Runtime verification: build x64 Debug -> run EditorTool -> full-screen capture (sandbox disabled) -> remove temp test code.
- Synthetic mouse/keyboard automation of ImGui is unreliable; verify via temp code (auto-create/select objects, ADDLOG results).
- Crash debugging: GenerateMapFile (on) + event log offset -> map symbol lookup.

## Dependencies

- DirectX 11 SDK (Windows SDK), DirectXTex
- ImGui + ImGuizmo (editor UI/gizmo, in Engine)
- ufbx (FBX import, single-file, in Libraries/Include/ufbx)
- magic_enum (present but unused — EnumToString is a static switch)
