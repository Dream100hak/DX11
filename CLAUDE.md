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
|   +-- Shader.h/.cpp       # Legacy FX11 shader (Terrain/SSAO only)
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
- FX11 -> native HLSL migration mostly done (Terrain already HLSL incl. HS/DS). Remaining FX: SSAO, particles (Fire/Rain, Stream-Output), and model shadow/SSAO + Standard/Thumbnail FX `_shader` (see Current Progress).
- `HlslShader`: VS/PS/HS/DS/GS/CS compile, auto InputLayout creation, cbuffer Push methods, DrawLineIndexed
- `Shader`: Legacy FX11 wrapper (SSAO, particles, and not-yet-removed model shadow/ssao overrides)

### Rendering Flow
```
Camera::Render_Forward()
  -> RenderContext setup (tech, view, proj, lightArray)
  -> Opaque: Front-to-Back sort (Early-Z)
  -> Transparent: Back-to-Front sort (alpha blending)
  -> InstancingManager::Render(RenderContext, gameObjects)
    -> MeshRenderer::Draw(RenderContext)
      -> HlslShader::Bind() -> Push*Data(b0~b4) -> SetPSSRV(t0~t4) -> DrawIndexed()
```

### Constant Buffer Layout
- `b0`: GlobalData (V, P, VP, VInv, Shadow, Time)
- `b1`: TransformData (World)
- `b2`: LightData (Ambient, Diffuse, Specular, Direction)
- `b3`: MaterialData (Mat properties, UseTexture, UseAlphaClip)
- `b4`: BoneData (Transforms[250])

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

## Current Progress (Deferred + FX11 removal in progress, ~commit 53 + Stage 4)

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

### Next Steps (to finish FX11 removal)
- SSAO itself FX->HLSL (VS/PS only; not wired into deferred lighting yet, ssao map shows full-red). Also: terrain writes only depth (no normals) into the SSAO normal-depth map — `TerrainRendererNotPS` shares the PS-less `Terrain_Shadow_HLSL`, but FX-era SsaoNormalDepth T1 had a PS writing normal+depth. Needs a Terrain normal-depth PS variant when SSAO is finished.
- ~~Shadow UX: light shadow bounds fixed at origin~~ **FIXED**: shadow sphere now auto-fits to the camera focus point each frame (`Light::UpdateMatrix` — `_autoFitShadow`, default on, toggleable in inspector) with light-space texel snapping to prevent shadow shimmer while the camera moves. Verified: model at (150,0,-10) (formerly outside bounds, shadowless) now casts a proper shadow.
- Particles (Fire/Rain/RainSO): need Stream-Output support in HlslShader.
- Then remove `Shader.h`/FX11 + TextureRenderer(FX).
- Editor gap: clip (.clip) has no scene drag-drop source (FolderContents CLIP branch lacks `DragModelFileToGUIWnd`); `CreateModelAnimatorMesh`/SceneWindow CLIP-drop branch already added, just needs the drag source.

## Known Issues

### Engine
- Remaining FX11: SSAO compute/blur (`Ssao`/`SsaoBlur`) + particles (Fire/Rain/RainSO, need Stream-Output in HlslShader) + shared includes (`00. Global/Light/Render.fx`). Model/shadow/SSAO-normal-depth all HLSL now (Stage 4~5). NOTE: Terrain is already HLSL (HS/DS supported).
- Material Sampler nullptr temp binding (needs RenderStateManager integration)
- Deferred Pass 3 (skybox/transparent) may misalign vs GBuffer depth when scene window has x/y offset (opaque unaffected).

### Editor
- `Hiearchy.cpp`: ImGui::Selectable comma operator bug (isSelected ignored)
- `GetEditorWindow()`: No return value when key not found (UB)
- `wstring->string` conversion breaks Korean characters
- Thumbnail cache has no size limit (memory leak risk)

## Build & Run

1. Open `Pot.sln` in Visual Studio
2. Configuration: x64 Debug or Release
3. Set EditorTool as startup project
4. Build and run

### Build structure (post header-xcopy removal)
- EditorTool references Engine via **ProjectReference** (auto lib link + correct build order; parallel `-m` builds work).
- EditorTool includes Engine headers **directly from `Engine\`** (include dirs: `$(SolutionDir)` + `$(SolutionDir)Engine\`). The old `xcopy *.h -> Libraries\Include\Engine` PreBuildEvent is gone — that copy caused stale-pch bugs (EditorTool compiling against outdated header copies).
- `Libraries\Include\` now holds **external** headers only (Assimp/DirectXTex/FX11/magic_enum). `Libraries\Lib\Engine\` (Engine.lib output) and `Libraries\Include\Engine\` are git-ignored.

## Dependencies

- DirectX 11 SDK (included in Windows SDK)
- FX11 (Effects 11) - for legacy shaders
- ImGui - editor UI
- Assimp - model import (AsConverter)
