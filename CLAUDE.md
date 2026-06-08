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

## Current Progress (Deferred + FX11 removal in progress, ~commit 52)

### Completed
- HlslShader wrapper, RenderStateManager, Frustum Culling, Render Queue, RenderContext, Multi-light (MAX_LIGHTS=16)
- **Deferred Rendering working** (GBuffer + multi-light lighting pass). Scene-view bug fixed: GBuffer now sized to actual scene viewport + `GBuffer::BindAsTarget` forces (0,0,w,h) origin viewport.
- **FX11 removal underway** (commits 47~52):
  - Deleted ~35 dead/migrated `.fx` (demo files, Sky, Terrain, Triangle, Outline, etc.)
  - Editor shaders SceneGrid/Collider/CubeMap/DebugTexture -> HLSL
  - **ModelRenderer/ModelAnimator -> HLSL**: static + animated models render in deferred GBuffer (`GBufferModel_HLSL`/`GBufferAnim_HLSL`); preview/thumbnails render lit via HLSL (`ModelPreview_HLSL`/`AnimPreview_HLSL` + `Thumbnail.hlsl PS_PreviewLit`). VS_Model uses per-mesh bone via `ModelBoneBuffer` (b5).
  - Fixed preview-corruption bug (FX preview render leaked render-state -> deferred scene models turned black; moving preview to HLSL fixed it).

### Next Steps (to finish FX11 removal)
- **Stage 4**: Model shadow/SSAO still use FX overrides (`Shadow`/`SsaoNormalDepth`) -> migrate to HLSL. (SsaoNormalDepth incl. terrain HS/DS = heaviest.)
- **Stage 5**: Remove ModelRenderer/ModelAnimator FX `_shader` member + caller FX args -> delete `Standard.fx`/`Thumbnail.fx`/`ShadowMap.fx`.
- SSAO itself FX->HLSL (VS/PS only; not wired into deferred lighting yet, ssao map shows full-red).
- Particles (Fire/Rain/RainSO): need Stream-Output support in HlslShader.
- Then remove `Shader.h`/FX11 + TextureRenderer(FX).
- Editor gap: clip (.clip) has no scene drag-drop source (FolderContents CLIP branch lacks `DragModelFileToGUIWnd`); `CreateModelAnimatorMesh`/SceneWindow CLIP-drop branch already added, just needs the drag source.

## Known Issues

### Engine
- Remaining FX11: SSAO (3), Standard/Thumbnail/ShadowMap (model shadow/ssao + FX `_shader`), particles (Fire/Rain, Stream-Output). NOTE: Terrain is already HLSL (HS/DS supported); the old "Terrain/SSAO can't migrate due to HS/DS" note was wrong.
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

## Dependencies

- DirectX 11 SDK (included in Windows SDK)
- FX11 (Effects 11) - for legacy shaders
- ImGui - editor UI
- Assimp - model import (AsConverter)
