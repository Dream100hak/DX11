#pragma once
#include "Component.h"
#include "Renderer.h"

class Transform;
class Camera;
class MeshRenderer;
class ModelRenderer;
class ModelAnimator;
class Light;
class BaseCollider;
class Terrain;
class Button;
class Billboard;

class Model;

// enum class로 생성되는 객체 타입 확인
enum class CreatedObjType : uint32
{
	GAMEOBJ,
	QUAD,
	CUBE,
	SPHERE,
	GRID,
	MODEL,
	TERRAIN,
	PARTICLE
};

class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	void Init();
	void Update();
	void Render();

	int32 CreateEmptyGameObject(CreatedObjType type = CreatedObjType::GAMEOBJ);
	void RemoveGameObject(int32 id);

	wstring FindEmptyName(CreatedObjType type);

	int32 CreateMesh(CreatedObjType type);
	int32 CreateModelMesh(shared_ptr<Model> model, Vec3 position = Vec3(0, 0, 0));
	int32 CreateModelAnimatorMesh(shared_ptr<Model> model, Vec3 position = Vec3(0, 0, 0), int32 animIndex = 0);
	int32 CreateLight(int32 lightType);

	// 에디터 표시용 enum -> 이름 (라이브러리 의존 없는 정적 switch — 추가 enum 은 분기 확장)
	template<typename E>
	static std::string EnumToString(E e)
	{
		if constexpr (std::is_same_v<E, CreatedObjType>)
		{
			switch (e)
			{
			case CreatedObjType::GAMEOBJ: return "GAMEOBJ";
			case CreatedObjType::QUAD: return "QUAD";
			case CreatedObjType::CUBE: return "CUBE";
			case CreatedObjType::SPHERE: return "SPHERE";
			case CreatedObjType::GRID: return "GRID";
			case CreatedObjType::MODEL: return "MODEL";
			case CreatedObjType::TERRAIN: return "TERRAIN";
			case CreatedObjType::PARTICLE: return "PARTICLE";
			default: return "(unnamed)";
			}
		}
		else if constexpr (std::is_same_v<E, ComponentType>)
		{
			switch (e)
			{
			case ComponentType::Transform: return "Transform";
			case ComponentType::Renderer: return "Renderer";
			case ComponentType::Camera: return "Camera";
			case ComponentType::Animator: return "Animator";
			case ComponentType::Light: return "Light";
			case ComponentType::Collider: return "Collider";
			case ComponentType::Terrain: return "Terrain";
			case ComponentType::Button: return "Button";
			case ComponentType::BillBoard: return "BillBoard";
			case ComponentType::SkyBox: return "SkyBox";
			case ComponentType::Script: return "Script";
			default: return "(unnamed)";
			}
		}
		else if constexpr (std::is_same_v<E, RendererType>)
		{
			switch (e)
			{
			case RendererType::Mesh: return "MeshRenderer";
			case RendererType::Model: return "ModelRenderer";
			case RendererType::Animator: return "ModelAnimator";
			case RendererType::Texture: return "TextureRenderer";
			case RendererType::Particle: return "ParticleSystem";
			default: return "(unnamed)";
			}
		}
		return "(unnamed)";
	}

	// 대신에 magic_enum 라이브러리 사용 가능 (선택 사항)
	/*
	template<typename E>
	static std::string EnumToString(E e)
	{
		auto name = magic_enum::enum_name(e);
		return name.empty() ? "(unnamed)" : std::string(name);
	}

	template<typename E>
	static std::optional<E> StringToEnum(std::string_view str)
	{
		return magic_enum::enum_cast<E>(str);
	}

	template<typename E>
	static bool EnumCombo(const char* label, E& currentValue)
	{
		bool changed = false;
		std::string preview = EnumToString(currentValue);

		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			for (auto [value, name] : magic_enum::enum_entries<E>())
			{
				bool isSelected = (currentValue == value);
				if (ImGui::Selectable(name.data(), isSelected))
				{
					currentValue = value;
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}
	*/

	ImVec2 WorldToScreenPos(const Vec3& world, const Matrix& mat)
	{
		ImVec2 position = ImVec2(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
		ImVec2 size = ImVec2(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height);

		// 4D 월드좌표 설정
		Vec4 worldPos4(world.x, world.y, world.z, 1.0f);
		// 월드좌표를 화면좌표로
		Vec4 trans = Vec4::Transform(worldPos4, mat);

		// w 나누기를 통한 관점 투영 변환
		if (std::abs(trans.w) < std::numeric_limits<float>::epsilon())
		{
			return ImVec2(-1, -1); // 카메라 뒤의 점
		}

		// 월드좌표를 화면좌표로
		trans /= trans.w;

		// 화면좌표 변환
		ImVec2 screenPos;
		screenPos.x = (trans.x + 1.0f) * 0.5f * size.x + position.x;
		screenPos.y = (1.0f - (trans.y + 1.0f) * 0.5f) * size.y + position.y;

		return screenPos;
	}

	bool IsHoveringWindow()
	{
		ImGuiContext& g = *ImGui::GetCurrentContext();
		ImGuiWindow* window = ImGui::FindWindowByName(GAME->GetSceneDesc().drawList->_OwnerName);
		if (g.HoveredWindow == window)   // Mouse hovering drawlist window
			return true;
		if (g.HoveredWindow != NULL) // Any other window is hovered
			return false;
		if (ImGui::IsMouseHoveringRect(window->InnerRect.Min, window->InnerRect.Max, false))   // Hovering drawlist window rect, while no other window is hovered (for _NoInputs windows)
			return true;
		return false;
	}

	bool CanActivate()
	{
		if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive())
		{
			return true;
		}
		return false;
	}
};
