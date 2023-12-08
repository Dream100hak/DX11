#pragma once
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
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
class SnowBillboard;


enum CreatedObjType
{
	GAMEOBJ,
	QUAD,
	CUBE,
	SPHERE,
	GRID,
	MODEL,
	TERRAIN,
};

BOOST_DESCRIBE_ENUM(CreatedObjType, GAMEOBJ, QUAD, CUBE, SPHERE, GRID, MODEL, TERRAIN)

class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	void Init();
	void Update();
	void Render();

	int32 CreateEmptyGameObject();
	void RemoveGameObject(int32 id);

	wstring FindEmptyName(CreatedObjType type);

	int32 CreateMesh(CreatedObjType type);

	template<class E>
	std::string EnumToString(E e)
	{
		string r = "(unnamed)";

		boost::mp11::mp_for_each<boost::describe::describe_enumerators<E>>([&](auto D)
			{
				if (e == D.value)
					r = D.name;
			});

		return r;
	}
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
		if (g.HoveredWindow != NULL)     // Any other window is hovered
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

