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
- **FX11 FULLY REMOVED** ??zero .fx files, no FX11 library/headers, no `Shader` class. Everything is native HLSL (`Shaders/HLSL/` + `HlslShader`).
- `HlslShader`: VS/PS/HS/DS/GS/CS compile, auto InputLayout creation, cbuffer Push methods, DrawLineIndexed, **Stream-Output GS** (`soEntries`/`soStride` in desc) + `DrawAuto` (particles)

### Rendering Flow (Deferred PBR + HDR, ~commit 68)
```
Camera::Render_Deferred()
  Pass 1: GBuffer fill (opaque, Front-to-Back) ??albedo+metallic / normal+roughness / worldpos+mask
          + Terrain::TerrainRendererGBuffer (?곕젅?몃룄 GBuffer 吏곹뻾, ?ъ썙???뱀닔寃쎈줈 ?놁쓬)
  Pass 2: DeferredLighting fullscreen -> HDR sceneColor(R16G16B16A16F, ???ш린) + GBuffer DSV
          Cook-Torrance(GGX+Smith+Schlick) 吏곸젒愿?+ IBL ?곕퉬?명듃(t5 irradiance/t6 prefiltered/t7 BRDF LUT)
          + Shadow(t3) + SSAO(t4)
  Pass 3: Background(?ㅼ뭅??z=far)/Transparent/Overlay -> sceneColor (GBuffer 源딆씠濡??щ컮瑜?李⑦룓)
  Bloom:  BrightPass(?섑봽) -> BlurH -> BlurV (b8 PostProcessBuffer)
  Pass 4: Tonemap(ACES+媛먮쭏, Bloom ?⑹꽦 t1) -> FXAA 耳쒕㈃ LDR 以묎컙踰꾪띁 -> FXAA -> 諛깅쾭??  + PassViewer ?ㅻ쾭?덉씠 (KEY_4 ?쒗솚 / ?щ럭 肄ㅻ낫), KEY_3 GBuffer 4遺꾪븷 ?붾쾭洹?```
- ?됯났媛? ?뚮쿋??GBuffer/?곕젅???ㅼ뭅????湲곕줉 ??linear 蹂?? ?ㅻℓ???⑥뒪媛 理쒖쥌 媛먮쭏 ?몄퐫??- IBL: `Engine/Ibl` ?쒖옉 ??1??踰좎씠??(`IblBake.hlsl` ??irradiance 32 ?먮툕 / prefiltered 128 ?먮툕 5mip / BRDF LUT 512). ?섍꼍留?= desertcube1024.dds (?ㅼ뭅?대컯?ㅼ? 怨듭쑀)
- ?ъ뒪?명봽濡쒖꽭???좉?/?뚮씪誘명꽣: Camera ?몄뒪?숉꽣 (Bloom on/off+Threshold+Intensity, FXAA)

### Constant Buffer Layout
- `b0`: GlobalData (V, P, VP, VInv, Shadow, Time)
- `b1`: TransformData (World)
- `b2`: LightData (Ambient, Diffuse, Specular, Direction)
- `b3`: MaterialData (Mat properties, UseTexture/UseAlphaClip/UseSsao + **Roughness/Metallic (PBR)**)
- `b4`: BoneData (Transforms[250])
- `b8`: ?⑥뒪蹂??꾩슜 (Terrain/Ssao/SsaoBlur/Particle/IblBake/Ibl/PostProcess/PassViewer)

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

## Current Progress (PBR/HDR/IBL/?ъ뒪?명봽濡쒖꽭??COMPLETE, ~commit 68)

### PBR ?뚮뜑留??꾪겕 (commits 64~68)
- **64. PBR 吏곸젒愿?*: GBuffer ?⑦궧 albedo.a=metallic / normal.w=roughness, DeferredLighting Phong->Cook-Torrance,
  MaterialDesc/MaterialBuffer(b3) roughness/metallic, Inspector PBR ?щ씪?대뜑, .mat ?щ㎎ ?뺤옣(`FileUtils::TryRead` 援щ쾭???명솚),
  Hiearchy "Create PBR Test Grid" 硫붾돱 (roughness x metallic 援ъ껜 洹몃━????PBR 寃利앹슜)
- **65. ?곕젅???뷀띁???몄엯 + HDR**: `Terrain.hlsl PS_GBuffer` + `Terrain_GBuffer_HLSL`, Terrain::Update ???ъ썙??吏곴렇由ш린 ?쒓굅
  (Camera Pass 1 ?먯꽌 `TerrainRendererGBuffer` ?몄텧). ?쇱씠???ㅼ뭅???щ챸 -> ???ш린 HDR sceneColor + Tonemap(ACES+媛먮쭏) 釉붾┸.
  **踰꾧렇?쎌뒪**: Pass 3 媛 諛깅쾭??RTV + GBuffer DSV 瑜??욎뼱 諛붿씤??-> ?ш린 遺덉씪移섎줈 OMSetRenderTargets 議곗슜???ㅽ뙣
  (?댁쟾???곕젅???ъ썙??源딆씠媛 硫붿씤 DSV ???덉뼱???곗뿰???숈옉). sceneColor/GBuffer DSV ?ш린 ?쇱튂濡??댁냼
- **66. ?щ럭 ?⑥뒪 酉곗뼱**: `PassViewer.hlsl` (Albedo/Normal/Roughness/Metallic/WorldPos/Depth/SSAO/Shadow), ?щ럭 醫뚯긽??肄ㅻ낫
  + KEY_4 ?쒗솚. Camera::RenderPassViewer 媛 ?ㅻℓ?????ㅻ쾭?덉씠
- **67. Bloom + FXAA**: `PostProcess.hlsl`(BrightPass soft-knee + 遺꾨━ 媛?곗떆?? ?섑봽 ?댁긽??ping-pong), `Fxaa.hlsl`(FXAA 3.11),
  Camera ?몄뒪?숉꽣 ?좉?. **鍮뚮뱶 踰꾧렇?쎌뒪**: Engine/EditorTool ??媛숈? IntDir 怨듭쑀 -> pch.obj 異⑸룎??諛섎났 stale-pch ??洹쇰낯 ?먯씤,
  `$(ProjectName)` ?섏쐞濡?遺꾨━
- **68. IBL**: `IblBake.hlsl` + `Engine/Ibl` (?쒖옉 ??1?? irradiance 肄붿궗??而⑤낵猷⑥뀡 / GGX prefiltered mip 泥댁씤 / Karis BRDF LUT),
  DeferredLighting ?곕퉬?명듃媛 IBL ?뷀벂利??ㅽ럺?섎윭 (UseIbl=0 ?대갚 = 湲곗〈 ?쇱씠??ambient). ?ㅼ뭅?대컯?ㅻ? ?곗????먮툕留?SkyCubeMap)?쇰줈
  援먯껜??IBL ?섍꼍怨??쇱튂. 硫뷀깉由?癒명떚由ъ뼹???섍꼍 諛섏궗 (IBL ?꾩뿏 寃??????뺤긽?댁뿀??
- ?고???寃利??⑦꽩: EditorTool::Init ?꾩떆 援ъ껜 洹몃━??-> ?ㅽ겕由곗꺑 -> ?쒓굅. ?щ옒?쒕뒗 留곸빱 留?GenerateMapFile, 耳쒖졇 ?덉쓬) + ?대깽?몃줈洹?  ?ㅽ봽?뗭쑝濡??щ낵 ??텛??(68 ?먯꽌 SkyBox GameObject Transform ?꾨씫 null deref 瑜??닿구濡??≪쓬)

### Completed
- HlslShader wrapper, RenderStateManager, Frustum Culling, Render Queue, RenderContext, Multi-light (MAX_LIGHTS=16)
- **Deferred Rendering working** (GBuffer + multi-light lighting pass). Scene-view bug fixed: GBuffer now sized to actual scene viewport + `GBuffer::BindAsTarget` forces (0,0,w,h) origin viewport.
- **FX11 removal underway** (commits 47~52):
  - Deleted ~35 dead/migrated `.fx` (demo files, Sky, Terrain, Triangle, Outline, etc.)
  - Editor shaders SceneGrid/Collider/CubeMap/DebugTexture -> HLSL
  - **ModelRenderer/ModelAnimator -> HLSL**: static + animated models render in deferred GBuffer (`GBufferModel_HLSL`/`GBufferAnim_HLSL`); preview/thumbnails render lit via HLSL (`ModelPreview_HLSL`/`AnimPreview_HLSL` + `Thumbnail.hlsl PS_PreviewLit`). VS_Model uses per-mesh bone via `ModelBoneBuffer` (b5).
  - Fixed preview-corruption bug (FX preview render leaked render-state -> deferred scene models turned black; moving preview to HLSL fixed it).
  - **Stage 4 done ??model shadow + SSAO normal-depth -> HLSL**:
    - `RenderContext` gained `shadowPass`/`ssaoPass` flags (mirror `deferredPass`); ShadowMap.cpp/Ssao.cpp set the flag instead of FX `shaderOverride`.
    - Shadow: `Shadow_HLSL`/`ShadowModel_HLSL`/`ShadowAnim_HLSL` reuse `Standard_VS` (VS_Mesh/Model/Animation) + `ShadowMap_PS PS_AlphaClip`; light VP fed via PushGlobalData(lightV, lightP). New `RasterizerStateType::ShadowDepth` (DepthBias 100000, SlopeScaled 1.0) replaces FX `Depth` RS to avoid acne. PS_AlphaClip now guards on `UseTexture && UseAlphaClip` (FX used PS=NULL = no clip, so untextured objects still cast shadows).
    - SSAO: new `Shaders/HLSL/SsaoNormalDepth.hlsl` (VS_Mesh/Model/Animation + PS_Main) outputs view-space normal + view-space depth -> `SsaoNormalDepth_HLSL`/`...Model_HLSL`/`...Anim_HLSL` (FX wrote world-space normal ??HLSL is more correct).
    - InstancingManager tween (b6) push selects shader by pass: deferred->GBufferAnim, shadow->ShadowAnim, ssao->SsaoNormalDepthAnim.
    - Renderer shadow/ssao Draw branches added in MeshRenderer/ModelRenderer/ModelAnimator (mirror deferred; per-mesh bone b5 for static, TransformMap t5 for anim).
    - Build clean (x64 Debug). NOTE: `ShadowMap_VS.hlsl` now dead (Shadow_HLSL repointed to Standard_VS) but left in tree/vcxproj; FX `Shadow`/`SsaoNormalDepth` resources no longer consumed (FX `Shadow` still registered). Delete in Stage 5.
    - **Runtime VERIFIED** (screenshot test with Kachujin static + animated models): both cast sharp character-shaped shadows onto terrain in the deferred scene; both render into the SSAO normal-depth map with view-space normals. "Models don't get shadows" reports are a **light shadow-bounds coverage issue, not a shader bug**: Light's shadow bounding sphere defaults to center=(0,0,0) radius=150, while the usual editing area (terrain path, camera spawn x??81) sits at/outside that boundary ??casters outside the sphere are clipped from the light's ortho frustum and receivers fall outside the shadow-UV guard. Adjust via Direction Light inspector (Shadow Bounding Box Center/Radius).

  - **Stage 5 done ??model FX `_shader` removed, 4 .fx deleted**:
    - ModelRenderer/ModelAnimator: no-arg ctors, `_shader`/`ChangeShader`/`PushMeshes`/`PushBufferInstancing`/FX Draw fallbacks removed; preview HLSL path is now the unconditional forward tail. `GetInstanceID` = (model, 0).
    - Deleted `01. Standard.fx`, `01. Thumbnail.fx`, `00. ShadowMap.fx`, `00. SsaoNormalDepth.fx`, dead `ShadowMap_VS.hlsl` (+ vcxproj/filters entries). FX registrations `Standard`/`Thumbnail`/`Shadow` removed from ResourceManager.
    - `Material::Load`: "Standard" shader string -> `Standard_HLSL` only (no FX compile); AsConverter/FolderContents write a literal `"01. Standard.fx"` for .mat format compat.
    - InstancingManager pushes tween (b6) to the pass-appropriate HLSL shader incl. `AnimPreview_HLSL`; ModelAnimator self-pushes tween only for single-instance draws (preview) to avoid clobbering the instanced array.
    - **Fixed latent UB crash**: `GameObject::GetMeshRenderer/GetModelRenderer/GetModelAnimator` blindly static_cast the shared `ComponentType::Renderer` slot ??Camera::SortGameObject read a garbage Material through a ModelRenderer-as-MeshRenderer cast (previously masked because `_shader` occupied that memory offset). Now type-checked via `GetRenderType()` before casting.
    - Runtime verified: editor stable 60s+ with a model in scene; deferred render + shadow OK.
  - **SSAO -> HLSL + deferred wiring done**:
    - New `Shaders/HLSL/Ssao.hlsl` (14-sample hemisphere AO) + `SsaoBlur.hlsl` (bilateral edge-preserving blur; horizontal/vertical via cbuffer `HorzBlur` instead of FX uniform-bool techniques). Deleted `00. Ssao.fx`/`00. SsaoBlur.fx`. FX samplers (BORDER 1e5 / WRAP / CLAMP) recreated in `Ssao::CreateSamplers` and bound via `SetPSSampler`.
    - SSAO cbuffers bound at b8 (VS needs FrustumCorners too); topology set explicitly (terrain leaves PATCHLIST); ping-pong input SRV unbound after each blur draw.
    - **Terrain normal-depth gap fixed**: `Terrain.hlsl PS_NormalDepth` (view-space normal + depth from heightmap finite differences) -> `Terrain_NormalDepth_HLSL`; `Terrain::TerrainRendererNormalDepth` used by the SSAO pass (was PS-less depth-only).
    - **Deferred lighting now consumes SSAO**: `DeferredLighting.hlsl` samples `SsaoMap` (t4) and multiplies the ambient term when `UseSsao`; Camera Pass 2 binds DefaultMaterial's ssao SRV and sets `useSsao`.
    - Runtime verified: ssao map shows real AO (ridge creases + model silhouette; previously solid red), depthNormal map now contains terrain normals, scene renders clean.
  - **Particles -> HLSL with Stream-Output (ALL .fx FILES NOW DELETED)**:
    - `HlslShaderDesc` gained `soEntries`/`soStride`/`soRasterize` -> `CreateGeometryShaderWithStreamOutput` (FX `ConstructGSWithSO` replacement); `HlslShader::DrawAuto`/`SetGSSampler`, `RenderStateManager::BindAllSamplersGS`, `BlendStateType::AdditiveSrcAlpha` (SrcAlpha/One ??FX Fire AdditiveBlending) added.
    - New `Fire.hlsl`/`Rain.hlsl` (VS_StreamOut+GS_StreamOut SO pass, VS_Draw+GS_Draw+PS_Draw billboard/line pass; FX cbFixed -> static const). Registered as `FireSO_HLSL`/`FireDraw_HLSL`/`RainSO_HLSL`/`RainDraw_HLSL` with states baked (SO: DisableDepth; FireDraw: AdditiveSrcAlpha+NoDepthWrite; RainDraw: NoDepthWrite).
    - ParticleSystem: FX effect-variable members -> `ParticleBuffer` CB (b8: EmitPos/GameTime/EmitDir/TimeStep), `Init(type, names, max)` (no shader arg); SO ping-pong flow kept; GS unbind + state restore after draw.
    - **GOTCHA**: `HlslShader::Draw/DrawIndexed` force TRIANGLELIST topology internally ??the SO kick-off draw must use raw `DCT->Draw(1,0)` to keep POINTLIST (otherwise the emitter never streams out and particles silently never appear).
    - Deleted ALL remaining .fx: `01. Fire/Rain/RainSO.fx` + shared includes `00. Global/Light/Render.fx` (+ vcxproj FxCompile group). `Shaders/` now contains only `HLSL/`.
    - Runtime verified: fire (additive flame billboards) + rain (falling line streaks) both render at 60fps.

  - **FX11 leftovers REMOVED (cleanup complete)**:
    - Deleted `Shader.h/.cpp`, `Pass.h/.cpp`, `Technique.h/.cpp` (Engine), `TextureRenderer.h/.cpp`, `Effects.h/.cpp` (EditorTool, fully dead) + FX11 lib/include dirs (`Libraries/Include/FX11`, `Libraries/Lib/FX11`).
    - `Material`: `_shader`/`SetShader`/`GetShader` removed ??HLSL only; `.mat` loader reads the shader string for format compat and always binds `Standard_HLSL`. MeshRenderer FX fallback tail removed (no HLSL shader = no draw). `RenderContext::shaderOverride` removed.
    - Billboard (dormant component) switched to `GetHlslShader` (needs a dedicated billboard HLSL shader if ever used).
    - GOTCHA: removing Effects11 lib broke `IID_ID3D11ShaderReflection` linkage (FX11 lib was providing the GUID) ??fixed by linking `dxguid.lib` in EnginePch.
    - Runtime verified: editor stable, deferred + shadow + ssao all render clean.

### Next Steps
- ~~?뚰떚??Transparent ???몄엯~~ ??**DONE in commit 70** (ParticleSystem ??Renderer(RendererType::Particle) ?뚯깮,
  Pass 3 ?먯꽌 HDR ?뚮뜑 ??遺덇퐙 Bloom + 源딆씠 李⑦룓 ?숈옉. SortGameObject 媛 Renderer ?щ’ 怨듭슜 getter ?ъ슜)
- ~~?꾨━酉??몃꽕??PBR~~ ??**DONE in commit 72** (Thumbnail.hlsl PS_PreviewLit 媛 Cook-Torrance + Reinhard/媛먮쭏 ?먯껜 泥섎━)
- IBL ?섍꼍留?援먯껜 UI (?꾩옱 desertcube1024.dds 怨좎젙; EnvIntensity ??Camera ?몄뒪?숉꽣???몄텧????commit 70)
- Shadow UX note: a camera-following shadow-sphere auto-fit was implemented then REVERTED by user preference (commit 58) ??shadow bounds are the fixed center/radius on the Light inspector; objects outside cast/receive no shadows.
- ~~.clip ?쒕옒洹몃뱶濡??뚯뒪~~ ??**DONE in commit 74** (FolderContents CLIP 遺꾧린??`DragModelFileToGUIWnd` ?곌껐)

## Known Issues

### Engine
- **FX11 is fully gone** ??all rendering is native HLSL via `HlslShader`.
- ~~Material Sampler nullptr temp binding~~ ???대? ?닿껐???덉뿀??(Material::Update 媛 `RENDER_STATES->BindAllSamplersPS()` ?ъ슜; 紐⑸줉???≪븯??寃?
- ~~Deferred Pass 3 misalign~~ ??**FIXED in commit 65** (HDR sceneColor + GBuffer DSV ?ш린 ?쇱튂)
- ~~?뚰떚??LDR 諛깅쾭??吏곴렇由ш린~~ ??**FIXED in commit 70** (Transparent ???몄엯)

### Editor
- ~~Hiearchy Selectable comma bug~~ / ~~GetEditorWindow UB~~ ???대? 怨쇨굅???섏젙???덉뿀??(紐⑸줉???≪븯??寃?
- ~~wstring<->string ?쒓? 源⑥쭚~~ ??**FIXED in commit 72** (Utils::ToString/ToWString ??UTF-8 蹂???ъ슜)
- ~~?몃꽕??罹먯떆 臾댁젣??~ ??**FIXED in commit 72** (FolderContents FIFO ?곹븳 64媛? ?좏깮 ??ぉ 蹂댄샇)
- ~~Inspector ?꾨━酉?留?`operator[]` 吏곸젒 ?몃뜳??~ ??**FIXED in commit 74** (Inspector ??MATERIAL 遺꾧린/
  PickMaterialTexture/GetMeshThumbnail ?꾨? find() 媛??+ null ?대갚)

## Build & Run

1. Open `Pot.sln` in Visual Studio
2. Configuration: x64 Debug or Release
3. Set EditorTool as startup project
4. Build and run

### Build structure (post header-xcopy removal)
- EditorTool references Engine via **ProjectReference** (auto lib link + correct build order; parallel `-m` builds work).
- EditorTool includes Engine headers **directly from `Engine\`** (include dirs: `$(SolutionDir)` + `$(SolutionDir)Engine\`). The old `xcopy *.h -> Libraries\Include\Engine` PreBuildEvent is gone ??that copy caused stale-pch bugs (EditorTool compiling against outdated header copies).
- `Libraries\Include\` now holds **external** headers only (Assimp/DirectXTex/magic_enum). `Libraries\Lib\Engine\` (Engine.lib output) and `Libraries\Include\Engine\` are git-ignored.

## Dependencies

- DirectX 11 SDK (included in Windows SDK)
- ImGui - editor UI
- Assimp - model import (AsConverter)
