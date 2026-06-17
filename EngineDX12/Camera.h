#pragma once
#include "Component.h"
#include "Frustum.h"
#include "imgui.h"

class GameObject;

enum class ProjectionType { Perspective, Orthographic };

// DX11 Engine/Camera.h 이식 (1차) — 뷰/프로젝션 보유 + 렌더 큐 분류(SortGameObject).
// 현재는 view/proj 를 외부(FlyCamera)에서 주입받음. 추후 Transform 기반 자체 계산 + Render_Deferred 이관.
class Camera : public Component
{
	using Super = Component;
public:
	Camera() : Component(ComponentType::Camera) {}

	void   SetView(const Matrix& v) { _matView = v; }
	void   SetProjection(const Matrix& p) { _matProjection = p; }

	virtual void OnInspectorGUI() override
	{
		const char* proj[] = { "Perspective", "Orthographic" };
		int p = (int)_projType;
		if (ImGui::Combo("Projection", &p, proj, 2)) _projType = (ProjectionType)p;
		ImGui::DragFloat("FOV", &_fov, 0.5f, 10.f, 120.f, "%.0f");
		ImGui::DragFloat("Near", &_near, 0.01f, 0.01f, 10.f, "%.2f");
		ImGui::DragFloat("Far", &_far, 1.f, 1.f, 5000.f, "%.0f");
		ImGui::TextDisabled("게임 카메라 — Play/Game 뷰 시점");
	}
	Matrix GetViewMatrix() { return _matView; }
	Matrix GetProjectionMatrix() { return _matProjection; }

	// 현재 씬(CUR_SCENE)의 렌더러를 렌더 큐별 버킷으로 분류
	void SortGameObject();

	vector<shared_ptr<GameObject>>& GetVecBackground()  { return _vecBackground; }
	vector<shared_ptr<GameObject>>& GetVecOpaque()       { return _vecOpaque; }
	vector<shared_ptr<GameObject>>& GetVecTransparent()  { return _vecTransparent; }
	const Frustum& GetFrustum() const { return _frustum; }

	// 프로젝션 파라미터 (자체 계산 이관 시 사용)
	float          _fov = 55.f;
	float          _near = 0.1f;
	float          _far = 200.f;
	ProjectionType _projType = ProjectionType::Perspective;

private:
	Matrix  _matView{};
	Matrix  _matProjection{};
	Frustum _frustum;

	vector<shared_ptr<GameObject>> _vecBackground;
	vector<shared_ptr<GameObject>> _vecOpaque;
	vector<shared_ptr<GameObject>> _vecTransparent;
};
