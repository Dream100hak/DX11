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

		// 라이트 색상 — diffuse 가 실제 빛 색(클러스터/포워드 radiance). ambient 는 환경 폴백.
		ImGui::ColorEdit3("Color", (float*)&_desc.diffuse);
		ImGui::ColorEdit3("Ambient", (float*)&_desc.ambient);

		ImGui::DragFloat("Intensity", &_intensity, 0.01f, 0.f, 100.f);
		SetIntensity(_intensity);

		if (_type == Point || _type == Spot)
		{
			ImGui::DragFloat("Range", &_range, 0.5f, 0.1f, 1000.f);
			ImGui::DragFloat3("Attenuation", (float*)&_attenuation, 0.001f, 0.f, 10.f);
			ImGui::Checkbox("Cast Shadows", &_castShadows);
		}

		if (_type == Spot)
		{
			ImGui::DragFloat("Spot Angle", &_spotAngleDeg, 0.5f, 1.f, 89.f);
		}

		if (_type == Directional)
		{
			ImGui::Separator();
			ImGui::Text("Shadow Bounding Box");
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
	void SetRange(float range) { _range = range; }
	float GetSpotAngleCos() const { return cosf(_spotAngleDeg * XM_PI / 180.f); }
	float GetSpotAngleDeg() const { return _spotAngleDeg; }
	void SetSpotAngleDeg(float deg) { _spotAngleDeg = deg; }
	Vec3 GetAttenuation() const { return _attenuation; }
	void SetAttenuation(const Vec3& att) { _attenuation = att; }
	float GetIntensity() const { return _intensity; }
	void SetIntensityValue(float v) { _intensity = v; _desc.intensity = v; }

	bool GetCastShadows() const { return _castShadows; }
	void SetCastShadows(bool v) { _castShadows = v; }

	// 점/스팟 그림자 슬롯 (PunctualShadowMap::Draw 가 매 프레임 할당, -1 = 없음)
	int32 GetShadowSlot() const { return _shadowSlot; }
	void  SetShadowSlot(int32 s) { _shadowSlot = s; }

	// 스팟 라이트 그림자용 원근 V/P (라이트 위치/방향/스팟각/거리 기준)
	Matrix GetSpotView();
	Matrix GetSpotProj();

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

	bool _castShadows = true; // 점/스팟 그림자 캐스트 여부
	int32 _shadowSlot = -1;   // 점/스팟 그림자 슬롯 (PunctualShadowMap 가 할당)

	float _depthBias = 100000;
	float _slopeScaledDepthBias = 1.0f;
	float _depthBiasClamp = 0.0f;

	ComPtr<ID3D11RasterizerState> _depthRS;

public:
	// 레거시 단일 섀도우 (포워드/프리뷰 호환용 — 캐스케이드 0 으로 채움)
	static Matrix S_MatView;
	static Matrix S_MatProjection;
	static Matrix S_Shadow;

	// Cascaded Shadow Maps — 메인 카메라 프러스텀을 분할해 매 프레임 갱신
	static Matrix S_CascadeView[CASCADE_COUNT];
	static Matrix S_CascadeProj[CASCADE_COUNT];
	static Matrix S_CascadeVPT[CASCADE_COUNT];      // V*P*T (디퍼드 샘플용)
	static float  S_CascadeSplitView[CASCADE_COUNT]; // 각 캐스케이드 far 의 카메라 뷰공간 거리

	// 스팟 라이트 그림자 — PunctualShadowMap::Draw 가 슬롯별 V*P*T 채움
	static Matrix S_SpotVPT[MAX_PUNCTUAL_SHADOWS];

private:
	void UpdateCascades(); // 디렉셔널: 메인 카메라 프러스텀 기반 캐스케이드 V/P 계산
};

