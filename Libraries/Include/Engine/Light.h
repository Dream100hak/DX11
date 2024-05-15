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

	virtual void Start();
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

		ImGui::Text("Shadow Bounding Box	");
		ImGui::DragFloat3("Center", (float*) & _center);
		ImGui::DragFloat("Radius" , &_radius);


		ImGui::Text("Depth Bias Settings");
		ImGui::DragFloat("DepthBias", &_depthBias, 1000.0f, 0.0f, FLT_MAX, "%.0f");
		ImGui::DragFloat("Slope Scaled Depth Bias", &_slopeScaledDepthBias, 0.1f, 0.0f, FLT_MAX, "%.3f");
		ImGui::DragFloat("Depth Bias Clamp", &_depthBiasClamp, 0.01f, 0.0f, FLT_MAX, "%.5f");

		CreateRasterizer();

		SetLightDirection();
	
		SetShadowBoundingSphere();
	}

public:
	LightDesc& GetLightDesc() { return _desc; }
	ComPtr<ID3D11RasterizerState> GetDepthRS() { return _depthRS; }

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

	void SetShadowBoundingSphere();

private:
	void CreateRasterizer();


private:
	LightDesc _desc;
	LightType _type = Directional;

	float _intensity = 1.f;  

	BoundingSphere _sceneBounds;
	Vec3 _center = Vec3::Zero;
	float _radius = 150.f;

	float _depthBias = 100000;
	float _slopeScaledDepthBias = 1.0f;
	float _depthBiasClamp = 0.0f;

	ComPtr<ID3D11RasterizerState> _depthRS;

public:
	static Matrix S_MatView;
	static Matrix S_MatProjection;
	static Matrix S_Shadow;

};

