#pragma once
#include "Common.h"
#include "Define.h"
#include "Component.h"
#include "Renderer.h"

// DX11 Engine/ImGuiManager 의 EnumToString 헬퍼 이식 (정적 switch, magic_enum 미사용).
// ImGui 백엔드 자체는 ImGuiDx12(_imgui) 가 담당. GUI 매크로로 접근.
class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	static const char* EnumToString(ComponentType t)
	{
		switch (t)
		{
		case ComponentType::Transform: return "Transform";
		case ComponentType::Renderer:  return "Renderer";
		case ComponentType::Camera:    return "Camera";
		case ComponentType::Animator:  return "Animator";
		case ComponentType::Light:     return "Light";
		case ComponentType::Collider:  return "Collider";
		case ComponentType::Terrain:   return "Terrain";
		case ComponentType::Button:    return "Button";
		case ComponentType::BillBoard: return "BillBoard";
		case ComponentType::SkyBox:    return "SkyBox";
		case ComponentType::Script:    return "Script";
		default:                       return "Unknown";
		}
	}
	static const char* EnumToString(RendererType t)
	{
		switch (t)
		{
		case RendererType::Mesh:     return "MeshRenderer";
		case RendererType::Model:    return "ModelRenderer";
		case RendererType::Animator: return "ModelAnimator";
		case RendererType::Texture:  return "TextureRenderer";
		case RendererType::Particle: return "ParticleSystem";
		case RendererType::Grid:     return "Grid";
		case RendererType::Foliage:  return "Foliage";
		case RendererType::Billboard:return "Billboard";
		default:                     return "Unknown";
		}
	}
};
