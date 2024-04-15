#pragma once
#include "Component.h"

enum class ProjectionType
{
	Perspective = 0, // 원근 투영
	Orthographic, // 직교 투영
};

class Camera : public Component
{
	using Super = Component;
public:
	Camera();
	virtual ~Camera();

	void OnInspectorGUI() override
	{
		Super::OnInspectorGUI();

		ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

		ImGui::DragFloat("Fov", (float*)&_fov, 0.01f);
		ImGui::DragFloat("Near", (float*)&_near, 0.01f);
		ImGui::DragFloat("Far", (float*)&_far, 0.01f);

		// ProjectionType 콤보 박스 추가
		const char* projectionTypes[] = { "Perspective", "Orthographic" };
		int currentProjection = static_cast<int>(_type); // 현재 선택된 ProjectionType을 int로 변환

		if (ImGui::Combo("Projection Type", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
		{
			_type = static_cast<ProjectionType>(currentProjection); // 사용자 선택에 따라 _type 업데이트
		}
	}


	virtual void Update() override;

	void SetProjectionType(ProjectionType type) { _type = type; }
	ProjectionType GetProjectionType() { return _type; }

	void UpdateMatrix();

	void SetNear(float value) { _near = value; }
	void SetFar(float value) { _far = value; }
	void SetFOV(float value) { _fov = value; }
	void SetWidth(float value) { _width = value; }
	void SetHeight(float value) { _height = value; }

	Matrix& GetViewMatrix() { return _matView; }
	Matrix& GetProjectionMatrix() { return _matProjection; }

	float GetWidth() { return _width; }
	float GetHeight() { return _height; }

	float GetFov() { return _fov; }
	float GetFar() { return _far; }

private:
	ProjectionType _type = ProjectionType::Perspective;
	Matrix _matView = Matrix::Identity;
	Matrix _matProjection = Matrix::Identity;

	float _near = 0.01f;
	float _far = 1000.f;
	float _fov = XM_PI / 4.f;
	float _width = 0.f;
	float _height = 0.f;


public:
	void SortGameObject();
	void Render_Forward();

	void SetCullingMaskLayerOnOff(uint8 layer, bool on)
	{
		if (on)
			_cullingMask |= (1 << layer);
		else
			_cullingMask &= ~(1 << layer);
	}

	void SetCullingMaskAll() { SetCullingMask(UINT32_MAX); }
	void SetCullingMask(uint32 mask) { _cullingMask = mask; }
	bool IsCulled(uint8 layer) { return (_cullingMask & (1 << layer)) != 0; }


private:
	uint32 _cullingMask = 0;
	vector<shared_ptr<GameObject>> _vecForward;
};