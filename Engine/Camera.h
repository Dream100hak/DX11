#pragma once
#include "Component.h"
#include "Frustum.h"

enum class ProjectionType
{
	Perspective = 0, // ���� ����
	Orthographic, // ���� ����
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

		// ProjectionType �޺� �ڽ� �߰�
		const char* projectionTypes[] = { "Perspective", "Orthographic" };
		int currentProjection = static_cast<int>(_type); // ���� ���õ� ProjectionType�� int�� ��ȯ

		if (ImGui::Combo("Projection Type", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
		{
			_type = static_cast<ProjectionType>(currentProjection); // ����� ���ÿ� ���� _type ������Ʈ
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
	void Render_Deferred();

public:
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
	vector<shared_ptr<GameObject>> _vecOpaque;
	vector<shared_ptr<GameObject>> _vecBackground;
	vector<shared_ptr<GameObject>> _vecTransparent;
	Frustum _frustum;
	shared_ptr<class GBuffer> _gBuffer;
	bool _showGBufferDebug = false;

	// HDR sceneColor — 디퍼드 라이팅/스카이/투명 패스가 그리는 씬 크기 오프스크린 버퍼.
	// GBuffer DSV 와 크기가 일치해 Pass 3 깊이 테스트가 올바르게 동작 (백버퍼+GBuffer DSV
	// 크기 불일치로 OMSetRenderTargets 가 조용히 실패하던 문제 해결). 톤매핑 패스가 백버퍼로 블릿.
	ComPtr<ID3D11Texture2D>          _sceneColorTex;
	ComPtr<ID3D11RenderTargetView>   _sceneColorRTV;
	ComPtr<ID3D11ShaderResourceView> _sceneColorSRV;
	void EnsureSceneColor(uint32 w, uint32 h);

	shared_ptr<class LightArrayDesc> CollectLights(shared_ptr<class Scene> scene);
};