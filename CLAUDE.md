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
- Migration from FX11 to native HLSL complete (Terrain/SSAO remain on FX due to HS/DS not supported)
- `HlslShader`: VS/PS compile, auto InputLayout creation, cbuffer Push methods
- `Shader`: Legacy FX11 wrapper (Terrain, SSAO only)

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

## Current Progress (Preparing Step 14)

### Completed (Step 1~13)
- HlslShader wrapper implementation
- RenderStateManager implementation
- FX -> HLSL migration (Standard, ShadowMap, Outline, Sky)
- Frustum Culling (p-vertex AABB)
- Render Queue (Opaque/Transparent separated sorting)
- RenderContext single entry point
- InstancingManager signature cleanup
- Multi-light support (MAX_LIGHTS=16)

### Next Steps
- **Step 14**: Deferred Rendering (Phase 2 start)
- RenderStateManager Sampler integration (currently nullptr temp binding)
- Rendering visual tests (texture, lighting verification)

## Known Issues

### Engine
- Terrain/SSAO shaders remain on FX11 (Hull/Domain Shader not supported, so not migrated)
- Material Sampler nullptr temp binding (needs RenderStateManager integration)
- Material creation missing auto HlslShader assignment (ImGuiManager::CreateMesh)

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
