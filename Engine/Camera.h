#pragma once
#include "Component.h"
#include "Frustum.h"
#include "BindShaderDesc.h" // PassViewerDesc + ConstantBuffer

enum class ProjectionType
{
	Perspective = 0, // 원근 투영 (관찰자 거리에 따라 크기 변화)
	Orthographic, // 정사영 투영 (거리에 관계없이 크기 일정)
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

		// 투영 타입 콤보 박스 선택
		const char* projectionTypes[] = { "Perspective", "Orthographic" };
		int currentProjection = static_cast<int>(_type); // 현재 타입을 ProjectionType에서 int로 변환

		if (ImGui::Combo("Projection Type", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes)))
		{
			_type = static_cast<ProjectionType>(currentProjection); // 콤보 박스 선택값을 _type에 저장
		}

		ImGui::SeparatorText("Post Processing");
		ImGui::Checkbox("Bloom", &_bloomEnabled);
		if (_bloomEnabled)
		{
			ImGui::DragFloat("Bloom Threshold", &_bloomThreshold, 0.01f, 0.f, 5.f);
			ImGui::DragFloat("Bloom Intensity", &_bloomIntensity, 0.01f, 0.f, 3.f);
		}
		ImGui::Checkbox("FXAA", &_fxaaEnabled);

		ImGui::Checkbox("Auto Exposure", &_exposureEnabled);
		if (_exposureEnabled)
			ImGui::DragFloat("Exposure Key", &_exposureKey, 0.005f, 0.01f, 2.f);

		ImGui::SeparatorText("IBL");
		ImGui::DragFloat("Env Intensity", &_envIntensity, 0.01f, 0.f, 4.f);

		ImGui::SeparatorText("Shadow (CSM)");
		ImGui::Checkbox("Cascade Debug Tint", &_csmDebug);

		ImGui::SeparatorText("Reflections (SSR)");
		ImGui::Checkbox("SSR", &_ssrEnabled);

		ImGui::SeparatorText("Volumetric (God Rays)");
		ImGui::Checkbox("Volumetric", &_volEnabled);
		if (_volEnabled)
		{
			ImGui::DragFloat("Vol Density", &_volDensity, 0.002f, 0.f, 1.f);
			ImGui::DragFloat("Vol Intensity", &_volIntensity, 0.02f, 0.f, 5.f);
			ImGui::DragFloat("Vol Scatter G", &_volScatterG, 0.01f, 0.f, 0.95f);
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
	float GetNear() { return _near; }
	float GetFar() { return _far; }

	// 중간 렌더 결과 시각화 (0=Final, PassViewerDesc 주석 참조)
	int32 GetDebugViewMode() const { return _debugViewMode; }
	void  SetDebugViewMode(int32 mode) { _debugViewMode = mode; }

	// Render_Deferred 최종 출력 오버라이드 — nullptr 이면 백버퍼 (Game 뷰가 자기 RT 지정)
	void SetFinalOutput(ComPtr<ID3D11RenderTargetView> rtv) { _finalRTV = rtv; }

	float GetEnvIntensity() const { return _envIntensity; }
	void  SetEnvIntensity(float v) { _envIntensity = v; }

	// 씬 뷰 와이어프레임 — GBuffer 지오메트리 패스만 와이어프레임으로 (씬뷰 툴바 토글)
	bool GetWireframe() const { return _wireframe; }
	void SetWireframe(bool v) { _wireframe = v; }

private:
	ProjectionType _type = ProjectionType::Perspective;
	Matrix _matView = Matrix::Identity;
	Matrix _matProjection = Matrix::Identity;

	float _near = 0.01f;
	float _far = 1000.f;
	float _fov = XM_PI / 4.f;
	float _width = 0.f;
	float _height = 0.f;

	ComPtr<ID3D11RenderTargetView> _finalRTV; // Game 뷰 등 외부 최종 타겟 (기본 백버퍼)


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

	// HDR sceneColor 텍스처와 스카이박스/지형 지오메트리가 모두 쓰는 렌더 타겟+깊이 버퍼.
	// GBuffer DSV와 렌더가 분리됨: Pass 3 스카이박스 렌더 시 렌더 타겟으로 사용하지만 (백스크린 GBuffer DSV
	// 렌더 영역 밖으로 OMSetRenderTargets가 조정되므로 깊이 버퍼 문제 해결됨). 풀스크린 지오메트리가 백스크린으로 출력됨.
	ComPtr<ID3D11Texture2D>          _sceneColorTex;
	ComPtr<ID3D11RenderTargetView>   _sceneColorRTV;
	ComPtr<ID3D11ShaderResourceView> _sceneColorSRV;
	void EnsureSceneColor(uint32 w, uint32 h);

	// 중간 렌더 결과 시각화
	int32 _debugViewMode = 0;
	shared_ptr<ConstantBuffer<PassViewerDesc>> _passViewerCB;
	void RenderPassViewer(const Matrix& V, const Matrix& P);

	// 포스트프로세싱: Bloom (핑퐁 텍스처) + FXAA (LDR 백버퍼 안티앨리어싱)
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

	bool _wireframe = false; // 씬 뷰 와이어프레임 토글

	// SSR (스크린스페이스 반사) — Pass 2 직후 sceneColor+GBuffer 로 반사 합성
	bool _ssrEnabled = true;
	ComPtr<ID3D11Texture2D>          _ssrTex;
	ComPtr<ID3D11RenderTargetView>   _ssrRTV;
	ComPtr<ID3D11ShaderResourceView> _ssrSRV;
	void RenderSSR(const Matrix& V, const Matrix& P, uint32 w, uint32 h);

	// Auto-exposure — 로그휘도 밉체인으로 평균 휘도 산출 → Tonemap 에서 노출 적용
	bool  _exposureEnabled = true;
	float _exposureKey = 0.18f;
	ComPtr<ID3D11Texture2D>          _lumTex;  // R16F, 풀 밉체인
	ComPtr<ID3D11RenderTargetView>   _lumRTV;  // mip0
	ComPtr<ID3D11ShaderResourceView> _lumSRV;
	shared_ptr<ConstantBuffer<struct ExposureDesc>> _exposureCB;
	void RenderLuminance(uint32 w, uint32 h);

	// 볼류메트릭 라이트(갓레이) — SSR 직후 sceneColor+CSM 로 인스캐터링 합성
	bool  _volEnabled = true;
	float _volDensity = 0.04f;
	float _volIntensity = 0.6f;
	float _volScatterG = 0.76f;
	ComPtr<ID3D11Texture2D>          _volTex;
	ComPtr<ID3D11RenderTargetView>   _volRTV;
	ComPtr<ID3D11ShaderResourceView> _volSRV;
	shared_ptr<ConstantBuffer<struct VolumetricDesc>> _volCB;
	void RenderVolumetric(const Matrix& V, const Matrix& P, uint32 w, uint32 h);

	// IBL (DeferredLighting b8)
	float _envIntensity = 1.f;
	shared_ptr<ConstantBuffer<IblDesc>> _iblCB;

	// CSM (DeferredLighting b9) — 캐스케이드 행렬/스플릿 전달
	bool _csmDebug = false; // 캐스케이드별 색 틴트 (디버그)
	shared_ptr<ConstantBuffer<struct CascadeDesc>> _cascadeCB;

	// 점/스팟 그림자 (DeferredLighting b10) — 스팟 V*P*T 전달
	shared_ptr<ConstantBuffer<struct PunctualShadowDesc>> _punctualCB;

	// 에디터 선택 아웃라인 (Outline b8) — editorInternal 카메라(씬 뷰)에서만 호출
	shared_ptr<ConstantBuffer<OutlineDesc>> _outlineCB;
	void RenderOutlinePass(const Matrix& V, const Matrix& P);

	shared_ptr<class LightArrayDesc> CollectLights(shared_ptr<class Scene> scene);
};
