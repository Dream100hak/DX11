#pragma once
// TODO: magic_enum.hpp를 Libraries/Include/에 추가한 후 주석 해제
// #include <magic_enum.hpp>
#include "Component.h"

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

// enum class로 변경하여 타입 안전성 확보
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

	// 임시 Enum → String 변환 (magic_enum 추가 전)
	template<typename E>
	static std::string EnumToString(E e)
	{
		// CreatedObjType만 지원
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
		return "(unnamed)";
	}

	// 나중에 magic_enum 추가 후 사용할 함수들 (주석 처리)
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

		// 4D 좌표로 변환
		Vec4 worldPos4(world.x, world.y, world.z, 1.0f);
		// 월드 좌표를 스크린 좌표로 변환
		Vec4 trans = Vec4::Transform(worldPos4, mat);

		// w 컴포넌트가 너무 작은 경우 스크린 좌표 변환을 하지 않음
		if (std::abs(trans.w) < std::numeric_limits<float>::epsilon())
		{
			return ImVec2(-1, -1); // 이 경우 스크린 밖으로 간주하고 유효하지 않은 좌표 반환
		}

		// 월드 좌표를 스크린 좌표로 변환한 뒤, w를 사용하여 정규화
		trans /= trans.w;

		// 스크린 좌표로 변환
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

	ImU32 GetColorU32(int idx)
	{
		IM_ASSERT(idx < COLOR::COUNT);
		return ImGui::ColorConvertFloat4ToU32(GAME->GetSceneDesc().style.Colors[idx]);
	}
};
