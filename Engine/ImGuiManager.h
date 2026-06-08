#pragma once
// TODO: magic_enum.hpp�� Libraries/Include/�� �߰��� �� �ּ� ����
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

// enum class�� �����Ͽ� Ÿ�� ������ Ȯ��
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

	// �ӽ� Enum �� String ��ȯ (magic_enum �߰� ��)
	template<typename E>
	static std::string EnumToString(E e)
	{
		// CreatedObjType�� ����
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

	// ���߿� magic_enum �߰� �� ����� �Լ��� (�ּ� ó��)
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

		// 4D ��ǥ�� ��ȯ
		Vec4 worldPos4(world.x, world.y, world.z, 1.0f);
		// ���� ��ǥ�� ��ũ�� ��ǥ�� ��ȯ
		Vec4 trans = Vec4::Transform(worldPos4, mat);

		// w ������Ʈ�� �ʹ� ���� ��� ��ũ�� ��ǥ ��ȯ�� ���� ����
		if (std::abs(trans.w) < std::numeric_limits<float>::epsilon())
		{
			return ImVec2(-1, -1); // �� ��� ��ũ�� ������ �����ϰ� ��ȿ���� ���� ��ǥ ��ȯ
		}

		// ���� ��ǥ�� ��ũ�� ��ǥ�� ��ȯ�� ��, w�� ����Ͽ� ����ȭ
		trans /= trans.w;

		// ��ũ�� ��ǥ�� ��ȯ
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
