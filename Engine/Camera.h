#pragma once
#include "Component.h"
#include "Frustum.h"
#include "BindShaderDesc.h" // PassViewerDesc + ConstantBuffer

enum class ProjectionType
{
	Perspective = 0, // 占쏙옙占쏙옙 占쏙옙占쏙옙
	Orthographic, // 占쏙옙占쏙옙 占쏙옙占쏙옙
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

		// ProjectionType 占쌨븝옙 占쌘쏙옙 占쌩곤옙
		const char* projectionTypes[] = { "Perspective", "Orthographic" };
		int currentProjection = static_cast<int>(_type); // 占쏙옙占쏙옙 占쏙옙占시듸옙 ProjectionType占쏙옙 int占쏙옙 占쏙옙환

		if (ImGui::Combo("Projection Type", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
		{
			_type = static_cast<ProjectionType>(currentProjection); // 占쏙옙占쏙옙占?占쏙옙占시울옙 占쏙옙占쏙옙 _type 占쏙옙占쏙옙占쏙옙트
		}

		ImGui::SeparatorText("Post Processing");
		ImGui::Checkbox("Bloom", &_bloomEnabled);
		if (_bloomEnabled)
		{
			ImGui::DragFloat("Bloom Threshold", &_bloomThreshold, 0.01f, 0.f, 5.f);
			ImGui::DragFloat("Bloom Intensity", &_bloomIntensity, 0.01f, 0.f, 3.f);
		}
		ImGui::Checkbox("FXAA", &_fxaaEnabled);

		ImGui::SeparatorText("IBL");
		ImGui::DragFloat("Env Intensity", &_envIntensity, 0.01f, 0.f, 4.f);
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

	// ?щ럭 ?⑥뒪 酉곗뼱 (0=Final, PassViewerDesc 二쇱꽍 李몄“)
	int32 GetDebugViewMode() const { return _debugViewMode; }
	void  SetDebugViewMode(int32 mode) { _debugViewMode = mode; }

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

	// HDR sceneColor ???뷀띁???쇱씠???ㅼ뭅???щ챸 ?⑥뒪媛 洹몃━?????ш린 ?ㅽ봽?ㅽ겕由?踰꾪띁.
	// GBuffer DSV ? ?ш린媛 ?쇱튂??Pass 3 源딆씠 ?뚯뒪?멸? ?щ컮瑜닿쾶 ?숈옉 (諛깅쾭??GBuffer DSV
	// ?ш린 遺덉씪移섎줈 OMSetRenderTargets 媛 議곗슜???ㅽ뙣?섎뜕 臾몄젣 ?닿껐). ?ㅻℓ???⑥뒪媛 諛깅쾭?쇰줈 釉붾┸.
	ComPtr<ID3D11Texture2D>          _sceneColorTex;
	ComPtr<ID3D11RenderTargetView>   _sceneColorRTV;
	ComPtr<ID3D11ShaderResourceView> _sceneColorSRV;
	void EnsureSceneColor(uint32 w, uint32 h);

	// ?щ럭 ?⑥뒪 酉곗뼱
	int32 _debugViewMode = 0;
	shared_ptr<ConstantBuffer<PassViewerDesc>> _passViewerCB;
	void RenderPassViewer(const Matrix& V, const Matrix& P);

	// ?ъ뒪?명봽濡쒖꽭????Bloom (?섑봽 ?댁긽??ping-pong) + FXAA (LDR 以묎컙 踰꾪띁)
	bool  _bloomEnabled = true;
	float _bloomThreshold = 1.0f;
	float _bloomIntensity = 0.6f;
	bool  _fxaaEnabled = true;
	ComPtr<ID3D11Texture2D>          _bloomTex[2];
	ComPtr<ID3D11RenderTargetView>   _bloomRTV[2];
	ComPtr<ID3D11ShaderResourceView> _bloomSRV[2];
	ComPtr<ID3D11Texture2D>          _ldrTex;
	ComPtr<ID3D11RenderTargetView>   _ldrRTV;
	ComPtr<ID3D11ShaderResourceView> _ldrSRV;
	shared_ptr<ConstantBuffer<PostProcessDesc>> _postCB;
	void RenderBloom(uint32 w, uint32 h);

	// IBL (DeferredLighting b8)
	float _envIntensity = 1.f;
	shared_ptr<ConstantBuffer<IblDesc>> _iblCB;

	shared_ptr<class LightArrayDesc> CollectLights(shared_ptr<class Scene> scene);
};