#pragma once
#include "Component.h"

enum LightType : uint8 
{
	Directional = 0,
	Point, 
	Spot,
};


class Light : public Component
{

	using Super = Component;

public:
	Light();
	virtual ~Light();

	virtual void Update();
	void UpdateMatrix();


	virtual void OnInspectorGUI() override
	{
		Super::OnInspectorGUI();

		ImGui::Text("Intentsity		");
		ImGui::SameLine();

		if (ImGui::DragFloat("##Intentsity", &_intensity))
		{
			SetIntensity(_intensity);
		}

		ImGui::Text("Mode		");
		ImGui::SameLine();

		int32 selected = (int32)_type;
		if (ImGui::Combo("LightMode", &selected, "Directional\0Point\0Spot\0"));
		{
			_type = static_cast<LightType>(selected);
		}

		SetLightDirection();
	}

public:
	LightDesc& GetLightDesc() { return _desc; }

	void SetLightDesc(LightDesc& desc) { _desc = desc; }

	void SetAmbient(const Color& color) { _desc.ambient = color; }
	void SetDiffuse(const Color& color) { _desc.diffuse = color; }
	void SetSpecular(const Color& color) { _desc.specular = color; }
	void SetEmissive(const Color& color) { _desc.emissive = color; }
	void SetLightDirection(Vec3 direction) { _desc.direction = direction; }
	void SetLightDirection()
	{ 
		_desc.direction = GetTransform()->GetRotation() ;		
	}
	void SetIntensity(float intensity){  _desc.intensity = _intensity; }


private:
	LightDesc _desc;
	LightType _type = Directional;

	float _intensity = 1.f;  

	BoundingSphere _sceneBounds;

public:
	static Matrix S_MatView;
	static Matrix S_MatProjection;
	static Matrix S_Shadow;
	

};

