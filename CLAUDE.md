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
- Visual Studio: File > Advanced Save Options > "Unicode (UTF-8 without signature) - Codepage 65001"
- VS Code: Click encoding in bottom-right > Select "UTF-8"
- Always verify UTF-8 encoding when creating new files

## Project Structure

```
DX11/
+-- Engine/                 # Core engine (static library)
|   +-- Graphics.h/.cpp     # D3D11 device/swapchain
|   +-- HlslShader.h/.cpp   # Native HLSL shader wrapper
|   +-- Material.h/.cpp     # Material system
|   +-- MeshRenderer.cpp    # Draw(RenderContext) single entry point
|   +-- ModelRenderer.cpp   # Static model renderer
|   +-- ModelAnimator.cpp   # Animated model renderer
|   +-- Camera.h/.cpp       # Camera + Render_Forward() pipeline
|   +-- Frustum.h/.cpp      # Frustum Culling (p-vertex AABB)
|   +-- InstancingManager.* # Instancing manager
|   +-- RenderContext.h     # Render pass context struct
|   +-- Scene.h/.cpp        # Scene management + GetLights()
|   +-- ...                 # GameObject, Transform, Physics, ResourceManager etc.
+-- EditorTool/             # Editor application
|   +-- EditorTool.h/.cpp   # Editor entry point
|   +-- EditorToolManager.* # Editor window management
|   +-- Inspector.h/.cpp    # Property inspector
|   +-- Hiearchy.h/.cpp     # Scene hierarchy (typo: should be Hierarchy)
|   +-- FolderContents.*    # Project file browser
|   +-- ImGuiManager.*      # ImGui integration
|   +-- ...
+-- Shaders/
|   +-- HLSL/               # HLSL shader files
|       +-- Common.hlsli    # Shared cbuffer/struct definitions
|       +-- Lighting.hlsli  # Lighting functions (multi-light support)
|       +-- Standard_VS/PS.hlsl  # Main rendering shader
|       +-- ShadowMap_VS/PS.hlsl
|       +-- Outline_VS/PS.hlsl
|       +-- Sky_VS/PS.hlsl
|       +-- ...
+-- Libraries/
|   +-- Include/            # External library headers
|   +-- Lib/                # External library binaries
+-- Pot.sln                 # Visual Studio solution
```

## Architecture & Rendering Pipeline

### Shader System
- **FX11 FULLY REMOVED** — zero .fx files, no FX11 library/headers, no `Shader` class. Everything is native HLSL (`Shaders/HLSL/` + `HlslShader`).
- `HlslShader`: VS/PS/HS/DS/GS/CS compile, auto InputLayout creation, cbuffer Push methods, DrawLineIndexed, **Stream-Output GS** (`soEntries`/`soStride` in desc) + `DrawAuto` (particles)

### Rendering Flow (Deferred PBR + HDR, ~commit 68)
```
Camera::Render_Deferred()
  Pass 1: GBuffer fill (opaque, Front-to-Back) — albedo+metallic / normal+roughness / worldpos+mask
          + Terrain::TerrainRendererGBuffer (터레인도 GBuffer 직행, 포워드 특수경로 없음)
  Pass 2: DeferredLighting fullscreen -> HDR sceneColor(R16G16B16A16F, 씬 크기) + GBuffer DSV
          Cook-Torrance(GGX+Smith+Schlick) 직접광 + IBL 앰비언트(t5 irradiance/t6 prefiltered/t7 BRDF LUT)
          + Shadow(t3) + SSAO(t4)
  Pass 3: Background(스카이 z=far)/Transparent/Overlay -> sceneColor (GBuffer 깊이로 올바른 차폐)
  Bloom:  BrightPass(하프) -> BlurH -> BlurV (b8 PostProcessBuffer)
  Pass 4: Tonemap(ACES+감마, Bloom 합성 t1) -> FXAA 켜면 LDR 중간버퍼 -> FXAA -> 백버퍼
  + PassViewer 오버레이 (KEY_4 순환 / 씬뷰 콤보), KEY_3 GBuffer 4분할 디버그
```
- 색공간: 알베도(GBuffer/터레인/스카이)는 기록 시 linear 변환, 톤매핑 패스가 최종 감마 인코딩
- IBL: `Engine/Ibl` 시작 시 1회 베이크 (`IblBake.hlsl` — irradiance 32 큐브 / prefiltered 128 큐브 5mip / BRDF LUT 512). 환경맵 = desertcube1024.dds (스카이박스와 공유)
- 포스트프로세싱 토글/파라미터: Camera 인스펙터 (Bloom on/off+Threshold+Intensity, FXAA)

### Constant Buffer Layout
- `b0`: GlobalData (V, P, VP, VInv, Shadow, Time)
- `b1`: TransformData (World)
- `b2`: LightData (Ambient, Diffuse, Specular, Direction)
- `b3`: MaterialData (Mat properties, UseTexture/UseAlphaClip/UseSsao + **Roughness/Metallic (PBR)**)
- `b4`: BoneData (Transforms[250])
- `b8`: 패스별 전용 (Terrain/Ssao/SsaoBlur/Particle/IblBake/Ibl/PostProcess/PassViewer)

### Multi-Light
- `MAX_LIGHTS = 16` (Define.h)
- `LightArrayDesc` (BindShaderDesc.h)
- `ComputeDirectionalLightArray()` (Lighting.hlsli)

### Render Queue
- `RenderQueue` enum: Background / Opaque / AlphaTest / Transparent / Overlay
- Set via `_renderQueue` field on Material

## Coding Conventions

### Naming
- Member variables: `_camelCase` (underscore prefix)
- Classes: `PascalCase`
- Macros: `ALL_CAPS`
- HLSL semantics: `POSITION`, `NORMAL`, `TEXCOORD`, `INST_WORLD` etc.

### Macros (Define.h)
- `GRAPHICS`, `INPUT`, `TIME`, `RESOURCES`, `TOOL` - singleton access macros
- `SELECTED_H`, `SELECTED_P` - state access macros

### Shader Conventions
- VS/PS separated: `{Name}_VS.hlsl`, `{Name}_PS.hlsl`
- Shared code: include `Common.hlsli`, `Lighting.hlsli`
- Sampler: s0~s4 (DiffuseMap, SpecularMap, NormalMap, ShadowMap, SsaoMap)
- Texture SRV: t0~t4 mapping

## Current Progress (PBR/HDR/IBL/포스트프로세싱 COMPLETE, ~commit 68)

### PBR 렌더링 아크 (commits 64~68)
- **64. PBR 직접광**: GBuffer 패킹 albedo.a=metallic / normal.w=roughness, DeferredLighting Phong->Cook-Torrance,
  MaterialDesc/MaterialBuffer(b3) roughness/metallic, Inspector PBR 슬라이더, .mat 포맷 확장(`FileUtils::TryRead` 구버전 호환),
  Hiearchy "Create PBR Test Grid" 메뉴 (roughness x metallic 구체 그리드 — PBR 검증용)
- **65. 터레인 디퍼드 편입 + HDR**: `Terrain.hlsl PS_GBuffer` + `Terrain_GBuffer_HLSL`, Terrain::Update 의 포워드 직그리기 제거
  (Camera Pass 1 에서 `TerrainRendererGBuffer` 호출). 라이팅/스카이/투명 -> 씬 크기 HDR sceneColor + Tonemap(ACES+감마) 블릿.
  **버그픽스**: Pass 3 가 백버퍼 RTV + GBuffer DSV 를 섞어 바인딩 -> 크기 불일치로 OMSetRenderTargets 조용히 실패
  (이전엔 터레인 포워드 깊이가 메인 DSV 에 있어서 우연히 동작). sceneColor/GBuffer DSV 크기 일치로 해소
- **66. 씬뷰 패스 뷰어**: `PassViewer.hlsl` (Albedo/Normal/Roughness/Metallic/WorldPos/Depth/SSAO/Shadow), 씬뷰 좌상단 콤보
  + KEY_4 순환. Camera::RenderPassViewer 가 톤매핑 뒤 오버레이
- **67. Bloom + FXAA**: `PostProcess.hlsl`(BrightPass soft-knee + 분리 가우시안, 하프 해상도 ping-pong), `Fxaa.hlsl`(FXAA 3.11),
  Camera 인스펙터 토글. **빌드 버그픽스**: Engine/EditorTool 이 같은 IntDir 공유 -> pch.obj 충돌이 반복 stale-pch 의 근본 원인,
  `$(ProjectName)` 하위로 분리
- **68. IBL**: `IblBake.hlsl` + `Engine/Ibl` (시작 시 1회: irradiance 코사인 컨볼루션 / GGX prefiltered mip 체인 / Karis BRDF LUT),
  DeferredLighting 앰비언트가 IBL 디퓨즈+스펙큘러 (UseIbl=0 폴백 = 기존 라이트 ambient). 스카이박스를 데저트 큐브맵(SkyCubeMap)으로
  교체해 IBL 환경과 일치. 메탈릭 머티리얼이 환경 반사 (IBL 전엔 검은색 — 정상이었음)
- 런타임 검증 패턴: EditorTool::Init 임시 구체 그리드 -> 스크린샷 -> 제거. 크래시는 링커 맵(GenerateMapFile, 켜져 있음) + 이벤트로그
  오프셋으로 심볼 역추적 (68 에서 SkyBox GameObject Transform 누락 null deref 를 이걸로 잡음)

### Completed
- HlslShader wrapper, RenderStateManager, Frustum Culling, Render Queue, RenderContext, Multi-light (MAX_LIGHTS=16)
- **Deferred Rendering working** (GBuffer + multi-light lighting pass). Scene-view bug fixed: GBuffer now sized to actual scene viewport + `GBuffer::BindAsTarget` forces (0,0,w,h) origin viewport.
- **FX11 removal underway** (commits 47~52):
  - Deleted ~35 dead/migrated `.fx` (demo files, Sky, Terrain, Triangle, Outline, etc.)
  - Editor shaders SceneGrid/Collider/CubeMap/DebugTexture -> HLSL
  - **ModelRenderer/ModelAnimator -> HLSL**: static + animated models render in deferred GBuffer (`GBufferModel_HLSL`/`GBufferAnim_HLSL`); preview/thumbnails render lit via HLSL (`ModelPreview_HLSL`/`AnimPreview_HLSL` + `Thumbnail.hlsl PS_PreviewLit`). VS_Model uses per-mesh bone via `ModelBoneBuffer` (b5).
  - Fixed preview-corruption bug (FX preview render leaked render-state -> deferred scene models turned black; moving preview to HLSL fixed it).
  - **Stage 4 done — model shadow + SSAO normal-depth -> HLSL**:
    - `RenderContext` gained `shadowPass`/`ssaoPass` flags (mirror `deferredPass`); ShadowMap.cpp/Ssao.cpp set the flag instead of FX `shaderOverride`.
    - Shadow: `Shadow_HLSL`/`ShadowModel_HLSL`/`ShadowAnim_HLSL` reuse `Standard_VS` (VS_Mesh/Model/Animation) + `ShadowMap_PS PS_AlphaClip`; light VP fed via PushGlobalData(lightV, lightP). New `RasterizerStateType::ShadowDepth` (DepthBias 100000, SlopeScaled 1.0) replaces FX `Depth` RS to avoid acne. PS_AlphaClip now guards on `UseTexture && UseAlphaClip` (FX used PS=NULL = no clip, so untextured objects still cast shadows).
    - SSAO: new `Shaders/HLSL/SsaoNormalDepth.hlsl` (VS_Mesh/Model/Animation + PS_Main) outputs view-space normal + view-space depth -> `SsaoNormalDepth_HLSL`/`...Model_HLSL`/`...Anim_HLSL` (FX wrote world-space normal — HLSL is more correct).
    - InstancingManager tween (b6) push selects shader by pass: deferred->GBufferAnim, shadow->ShadowAnim, ssao->SsaoNormalDepthAnim.
    - Renderer shadow/ssao Draw branches added in MeshRenderer/ModelRenderer/ModelAnimator (mirror deferred; per-mesh bone b5 for static, TransformMap t5 for anim).
    - Build clean (x64 Debug). NOTE: `ShadowMap_VS.hlsl` now dead (Shadow_HLSL repointed to Standard_VS) but left in tree/vcxproj; FX `Shadow`/`SsaoNormalDepth` resources no longer consumed (FX `Shadow` still registered). Delete in Stage 5.
    - **Runtime VERIFIED** (screenshot test with Kachujin static + animated models): both cast sharp character-shaped shadows onto terrain in the deferred scene; both render into the SSAO normal-depth map with view-space normals. "Models don't get shadows" reports are a **light shadow-bounds coverage issue, not a shader bug**: Light's shadow bounding sphere defaults to center=(0,0,0) radius=150, while the usual editing area (terrain path, camera spawn x≈181) sits at/outside that boundary — casters outside the sphere are clipped from the light's ortho frustum and receivers fall outside the shadow-UV guard. Adjust via Direction Light inspector (Shadow Bounding Box Center/Radius).

  - **Stage 5 done — model FX `_shader` removed, 4 .fx deleted**:
    - ModelRenderer/ModelAnimator: no-arg ctors, `_shader`/`ChangeShader`/`PushMeshes`/`PushBufferInstancing`/FX Draw fallbacks removed; preview HLSL path is now the unconditional forward tail. `GetInstanceID` = (model, 0).
    - Deleted `01. Standard.fx`, `01. Thumbnail.fx`, `00. ShadowMap.fx`, `00. SsaoNormalDepth.fx`, dead `ShadowMap_VS.hlsl` (+ vcxproj/filters entries). FX registrations `Standard`/`Thumbnail`/`Shadow` removed from ResourceManager.
    - `Material::Load`: "Standard" shader string -> `Standard_HLSL` only (no FX compile); AsConverter/FolderContents write a literal `"01. Standard.fx"` for .mat format compat.
    - InstancingManager pushes tween (b6) to the pass-appropriate HLSL shader incl. `AnimPreview_HLSL`; ModelAnimator self-pushes tween only for single-instance draws (preview) to avoid clobbering the instanced array.
    - **Fixed latent UB crash**: `GameObject::GetMeshRenderer/GetModelRenderer/GetModelAnimator` blindly static_cast the shared `ComponentType::Renderer` slot — Camera::SortGameObject read a garbage Material through a ModelRenderer-as-MeshRenderer cast (previously masked because `_shader` occupied that memory offset). Now type-checked via `GetRenderType()` before casting.
    - Runtime verified: editor stable 60s+ with a model in scene; deferred render + shadow OK.
  - **SSAO -> HLSL + deferred wiring done**:
    - New `Shaders/HLSL/Ssao.hlsl` (14-sample hemisphere AO) + `SsaoBlur.hlsl` (bilateral edge-preserving blur; horizontal/vertical via cbuffer `HorzBlur` instead of FX uniform-bool techniques). Deleted `00. Ssao.fx`/`00. SsaoBlur.fx`. FX samplers (BORDER 1e5 / WRAP / CLAMP) recreated in `Ssao::CreateSamplers` and bound via `SetPSSampler`.
    - SSAO cbuffers bound at b8 (VS needs FrustumCorners too); topology set explicitly (terrain leaves PATCHLIST); ping-pong input SRV unbound after each blur draw.
    - **Terrain normal-depth gap fixed**: `Terrain.hlsl PS_NormalDepth` (view-space normal + depth from heightmap finite differences) -> `Terrain_NormalDepth_HLSL`; `Terrain::TerrainRendererNormalDepth` used by the SSAO pass (was PS-less depth-only).
    - **Deferred lighting now consumes SSAO**: `DeferredLighting.hlsl` samples `SsaoMap` (t4) and multiplies the ambient term when `UseSsao`; Camera Pass 2 binds DefaultMaterial's ssao SRV and sets `useSsao`.
    - Runtime verified: ssao map shows real AO (ridge creases + model silhouette; previously solid red), depthNormal map now contains terrain normals, scene renders clean.
  - **Particles -> HLSL with Stream-Output (ALL .fx FILES NOW DELETED)**:
    - `HlslShaderDesc` gained `soEntries`/`soStride`/`soRasterize` -> `CreateGeometryShaderWithStreamOutput` (FX `ConstructGSWithSO` replacement); `HlslShader::DrawAuto`/`SetGSSampler`, `RenderStateManager::BindAllSamplersGS`, `BlendStateType::AdditiveSrcAlpha` (SrcAlpha/One — FX Fire AdditiveBlending) added.
    - New `Fire.hlsl`/`Rain.hlsl` (VS_StreamOut+GS_StreamOut SO pass, VS_Draw+GS_Draw+PS_Draw billboard/line pass; FX cbFixed -> static const). Registered as `FireSO_HLSL`/`FireDraw_HLSL`/`RainSO_HLSL`/`RainDraw_HLSL` with states baked (SO: DisableDepth; FireDraw: AdditiveSrcAlpha+NoDepthWrite; RainDraw: NoDepthWrite).
    - ParticleSystem: FX effect-variable members -> `ParticleBuffer` CB (b8: EmitPos/GameTime/EmitDir/TimeStep), `Init(type, names, max)` (no shader arg); SO ping-pong flow kept; GS unbind + state restore after draw.
    - **GOTCHA**: `HlslShader::Draw/DrawIndexed` force TRIANGLELIST topology internally — the SO kick-off draw must use raw `DCT->Draw(1,0)` to keep POINTLIST (otherwise the emitter never streams out and particles silently never appear).
    - Deleted ALL remaining .fx: `01. Fire/Rain/RainSO.fx` + shared includes `00. Global/Light/Render.fx` (+ vcxproj FxCompile group). `Shaders/` now contains only `HLSL/`.
    - Runtime verified: fire (additive flame billboards) + rain (falling line streaks) both render at 60fps.

  - **FX11 leftovers REMOVED (cleanup complete)**:
    - Deleted `Shader.h/.cpp`, `Pass.h/.cpp`, `Technique.h/.cpp` (Engine), `TextureRenderer.h/.cpp`, `Effects.h/.cpp` (EditorTool, fully dead) + FX11 lib/include dirs (`Libraries/Include/FX11`, `Libraries/Lib/FX11`).
    - `Material`: `_shader`/`SetShader`/`GetShader` removed — HLSL only; `.mat` loader reads the shader string for format compat and always binds `Standard_HLSL`. MeshRenderer FX fallback tail removed (no HLSL shader = no draw). `RenderContext::shaderOverride` removed.
    - Billboard (dormant component) switched to `GetHlslShader` (needs a dedicated billboard HLSL shader if ever used).
    - GOTCHA: removing Effects11 lib broke `IID_ID3D11ShaderReflection` linkage (FX11 lib was providing the GUID) — fixed by linking `dxguid.lib` in EnginePch.
    - Runtime verified: editor stable, deferred + shadow + ssao all render clean.

### Next Steps
- ~~파티클 Transparent 큐 편입~~ — **DONE in commit 70** (ParticleSystem 이 Renderer(RendererType::Particle) 파생,
  Pass 3 에서 HDR 렌더 — 불꽃 Bloom + 깊이 차폐 동작. SortGameObject 가 Renderer 슬롯 공용 getter 사용)
- ~~프리뷰/썸네일 PBR~~ — **DONE in commit 72** (Thumbnail.hlsl PS_PreviewLit 가 Cook-Torrance + Reinhard/감마 자체 처리)
- IBL 환경맵 교체 UI (현재 desertcube1024.dds 고정; EnvIntensity 는 Camera 인스펙터에 노출됨 — commit 70)
- Shadow UX note: a camera-following shadow-sphere auto-fit was implemented then REVERTED by user preference (commit 58) — shadow bounds are the fixed center/radius on the Light inspector; objects outside cast/receive no shadows.
- Editor gap: clip (.clip) has no scene drag-drop source (FolderContents CLIP branch lacks `DragModelFileToGUIWnd`); `CreateModelAnimatorMesh`/SceneWindow CLIP-drop branch already added, just needs the drag source.

## Known Issues

### Engine
- **FX11 is fully gone** — all rendering is native HLSL via `HlslShader`.
- Material Sampler nullptr temp binding (needs RenderStateManager integration)
- ~~Deferred Pass 3 misalign~~ — **FIXED in commit 65** (HDR sceneColor + GBuffer DSV 크기 일치)
- ~~파티클 LDR 백버퍼 직그리기~~ — **FIXED in commit 70** (Transparent 큐 편입)

### Editor
- ~~Hiearchy Selectable comma bug~~ / ~~GetEditorWindow UB~~ — 이미 과거에 수정돼 있었음 (목록이 낡았던 것)
- ~~wstring<->string 한글 깨짐~~ — **FIXED in commit 72** (Utils::ToString/ToWString 이 UTF-8 변환 사용)
- ~~썸네일 캐시 무제한~~ — **FIXED in commit 72** (FolderContents FIFO 상한 64개, 선택 항목 보호)
- Inspector/FolderContents 가 프리뷰 맵을 `operator[]` 로 직접 인덱싱 — 키 부재 시 null 삽입 (썸네일 캐시 제거와
  맞물리면 잠재 크래시. 제거 시 선택 항목은 보호하지만 find() 가드로 바꾸는 게 안전)

## Build & Run

1. Open `Pot.sln` in Visual Studio
2. Configuration: x64 Debug or Release
3. Set EditorTool as startup project
4. Build and run

### Build structure (post header-xcopy removal)
- EditorTool references Engine via **ProjectReference** (auto lib link + correct build order; parallel `-m` builds work).
- EditorTool includes Engine headers **directly from `Engine\`** (include dirs: `$(SolutionDir)` + `$(SolutionDir)Engine\`). The old `xcopy *.h -> Libraries\Include\Engine` PreBuildEvent is gone — that copy caused stale-pch bugs (EditorTool compiling against outdated header copies).
- `Libraries\Include\` now holds **external** headers only (Assimp/DirectXTex/magic_enum). `Libraries\Lib\Engine\` (Engine.lib output) and `Libraries\Include\Engine\` are git-ignored.

## Dependencies

- DirectX 11 SDK (included in Windows SDK)
- ImGui - editor UI
- Assimp - model import (AsConverter)
