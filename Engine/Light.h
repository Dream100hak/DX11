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

		int32 selected = (int32)_type;
		if (ImGui::Combo("Light Type", &selected, "Directional\0Point\0Spot\0"))
			_type = static_cast<LightType>(selected);

		ImGui::DragFloat("Intensity", &_intensity, 0.01f, 0.f, 100.f);
		SetIntensity(_intensity);

		if (_type == Point || _type == Spot)
		{
			ImGui::DragFloat("Range", &_range, 0.5f, 0.1f, 1000.f);
			ImGui::DragFloat3("Attenuation", (float*)&_attenuation, 0.001f, 0.f, 10.f);
		}

		if (_type == Spot)
		{
			ImGui::DragFloat("Spot Angle", &_spotAngleDeg, 0.5f, 1.f, 89.f);
		}

		if (_type == Directional)
		{
			ImGui::Separator();
			ImGui::Text("Shadow Bounding Box");
			ImGui::Checkbox("Auto Fit To Camera", &_autoFitShadow);
			if (!_autoFitShadow)
				ImGui::DragFloat3("Center", (float*)&_center);
			ImGui::DragFloat("Radius", &_radius);

			ImGui::Text("Depth Bias Settings");
			ImGui::DragFloat("DepthBias", &_depthBias, 1000.0f, 0.0f, FLT_MAX, "%.0f");
			ImGui::DragFloat("Slope Scaled Depth Bias", &_slopeScaledDepthBias, 0.1f, 0.0f, FLT_MAX, "%.3f");
			ImGui::DragFloat("Depth Bias Clamp", &_depthBiasClamp, 0.01f, 0.0f, FLT_MAX, "%.5f");

			CreateRasterizer();
			SetLightDirection();
			SetShadowBoundingSphere();
		}
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

	LightType GetLightType() const { return _type; }
	void SetLightType(LightType type) { _type = type; }
	float GetRange() const { return _range; }
	float GetSpotAngleCos() const { return cosf(_spotAngleDeg * XM_PI / 180.f); }
	Vec3 GetAttenuation() const { return _attenuation; }

	void SetShadowBoundingSphere();

private:
	void CreateRasterizer();


private:
	LightDesc _desc;
	LightType _type = Directional;

	float _intensity = 1.f;
	float _range = 25.f;
	float _spotAngleDeg = 30.f;
	Vec3  _attenuation = Vec3(1.f, 0.09f, 0.032f);

	BoundingSphere _sceneBounds;
	Vec3 _center = Vec3::Zero;
	float _radius = 150.f;
	// 그림자 스피어를 매 프레임 카메라 포커스 지점으로 이동 (어디서 작업하든 그림자 유지)
	bool _autoFitShadow = true;
	static constexpr float SHADOW_MAP_SIZE = 2048.f; // EditorTool ShadowMap 해상도와 동일 (텍셀 스냅용)

	float _depthBias = 100000;
	float _slopeScaledDepthBias = 1.0f;
	float _depthBiasClamp = 0.0f;

	ComPtr<ID3D11RasterizerState> _depthRS;

public:
	static Matrix S_MatView;
	static Matrix S_MatProjection;
	static Matrix S_Shadow;

};

