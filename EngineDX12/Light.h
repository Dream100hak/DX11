#pragma once
#include "Component.h"
#include "imgui.h"

// DX11 Engine/Light 이식(1차) — 라이트 파라미터 보유 컴포넌트. Scene 이 ComponentType::Light 로 캐시.
enum class LightType { Directional, Point, Spot };

class Light : public Component
{
public:
	Light() : Component(ComponentType::Light) {}

	virtual void OnInspectorGUI() override
	{
		const char* kinds[] = { "Directional", "Point", "Spot" };
		int t = static_cast<int>(_lightType);
		if (ImGui::Combo("Type", &t, kinds, 3)) _lightType = static_cast<LightType>(t);
		ImGui::Checkbox("Enabled", &_enabled);
		ImGui::ColorEdit3("Color", &_color.x);
		ImGui::DragFloat("Intensity", &_intensity, 0.02f, 0.f, 16.f);
		if (_lightType != LightType::Point)
			ImGui::DragFloat3("Direction", &_direction.x, 0.01f);
		if (_lightType != LightType::Directional)
			ImGui::DragFloat("Range", &_range, 0.1f, 0.f, 100.f);
		if (_lightType == LightType::Spot)
			ImGui::DragFloat("Spot Angle", &_spotAngleDeg, 0.5f, 1.f, 89.f);
	}

	LightType _lightType = LightType::Directional;
	bool      _enabled = true;     // on/off (점/스팟 CB 게이트)
	Vec3      _color{ 1.f, 1.f, 1.f };
	float     _intensity = 1.f;
	Vec3      _direction{ 0.f, -1.f, 0.f };
	float     _range = 10.f;       // point/spot
	float     _spotAngleDeg = 30.f; // spot
};
