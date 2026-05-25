#include "pch.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "BindShaderDesc.h"
#include "Light.h"
#include "GBuffer.h"
#include "HlslShader.h"
#include "RenderStateManager.h"

shared_ptr<LightArrayDesc> Camera::CollectLights(shared_ptr<Scene> scene)
{
	auto lightArray = make_shared<LightArrayDesc>();
	lightArray->lightCount = 0;

	auto& lights = scene->GetLights();
	for (auto& lightObj : lights)
	{
		if (lightArray->lightCount >= MAX_LIGHTS) break;
		auto lc = lightObj->GetLight();
		if (!lc) continue;

		const LightDesc& ld = lc->GetLightDesc();
		LightData& data = lightArray->lights[lightArray->lightCount];

		data.diffuse    = ld.diffuse;
		data.ambient    = ld.ambient;
		data.intensity  = ld.intensity;
		data.type       = static_cast<int32>(lc->GetLightType());
		data.direction  = ld.direction;
		data.position   = lightObj->GetTransform()->GetPosition();
		data.range      = lc->GetRange();
		data.attenuation = lc->GetAttenuation();
		data.spotAngle  = lc->GetSpotAngleCos();

		lightArray->lightCount++;
	}
	return lightArray;
}

void Camera::SortGameObject()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	_vecForward.clear();
	_vecOpaque.clear();
	_vecBackground.clear();
	_vecTransparent.clear();

	// 매 프레임 절두체 갱신
	_frustum.Update(_matView * _matProjection);

	Vec3 camPos = GetTransform()->GetPosition();

	for (auto& gameObject : gameObjects)
	{
		if (IsCulled(gameObject->GetLayerIndex()))
			continue;

		auto renderer = gameObject->GetMeshRenderer()  ? static_pointer_cast<Renderer>(gameObject->GetMeshRenderer())
					  : gameObject->GetModelRenderer() ? static_pointer_cast<Renderer>(gameObject->GetModelRenderer())
					  : gameObject->GetModelAnimator() ? static_pointer_cast<Renderer>(gameObject->GetModelAnimator())
					: nullptr;

		if (renderer == nullptr)
			continue;

		// ✅ 핵심 수정: 절두체 컬링 전에 BoundingBox를 현재 월드 위치로 갱신
		renderer->TransformBoundingBox();  // ← 월드 행렬 기준으로 BoundingBox 갱신

		// Frustum Culling
		const BoundingBox& box = renderer->GetBoundingBox();
		bool hasValidBox = (box.Extents.x > 0.f || box.Extents.y > 0.f || box.Extents.z > 0.f);
		if (hasValidBox && !_frustum.IsInFrustum(box))
			continue;

		// RenderQueue 분류
		RenderQueue queue = RenderQueue::Opaque;
		if (auto mr = gameObject->GetMeshRenderer())
			if (mr->GetMaterial()) queue = mr->GetMaterial()->GetRenderQueue();

		if (static_cast<int32>(queue) >= static_cast<int32>(RenderQueue::Transparent))
			_vecTransparent.push_back(gameObject);
		else if (queue == RenderQueue::Background)
			_vecBackground.push_back(gameObject);
		else
			_vecOpaque.push_back(gameObject);

		_vecForward.push_back(gameObject);
	}

	// 불투명: Front-to-Back 정렬
	sort(_vecOpaque.begin(), _vecOpaque.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da < db;
		});

	// 투명: Back-to-Front 정렬
	sort(_vecTransparent.begin(), _vecTransparent.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da > db;
		});
}

void Camera::Render_Forward()
{
	auto cam   = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	auto scene = SCENE->GetCurrentScene();
	
	int32 tech = 0;
	if (INPUT->GetButton(KEY_TYPE::KEY_1)) tech = TECH_WIREFRAME;
	if (INPUT->GetButton(KEY_TYPE::KEY_2)) tech = TECH_CLOCKWISE;

	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();

	auto lightArray = CollectLights(scene);

	// RenderContext 구성
	RenderContext baseCtx;
	baseCtx.tech = tech;
	baseCtx.view   = V;
	baseCtx.proj   = P;
	baseCtx.light  = nullptr;
	baseCtx.lightArray = lightArray;
	baseCtx.shaderOverride = nullptr;
	baseCtx.hlslOverride   = nullptr;
	baseCtx.buffer = nullptr;

	// 1) 불투명 렌더 패스 (Front-to-Back, Depth Write ON)
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecOpaque);

	// 2) Background 렌더 패스 (스카이박스: 모든 불투명 지오메트리 이후, 투명 이전)
	//    SkyBoxDepth DSS (LESS_EQUAL + 깊이 쓰기 없음)로 빈 배경만 채움
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecBackground);

	// 3) 투명 렌더 패스 (Back-to-Front)
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecTransparent);
}

void Camera::Render_Deferred()
{
	auto cam   = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	auto scene = SCENE->GetCurrentScene();

	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();

	auto lightArray = CollectLights(scene);

	// G-Buffer lazy init
	uint32 w = static_cast<uint32>(_width);
	uint32 h = static_cast<uint32>(_height);
	if (!_gBuffer || _gBuffer->GetWidth() != w || _gBuffer->GetHeight() != h)
	{
		_gBuffer = make_shared<GBuffer>();
		_gBuffer->Init(w, h);
	}

	// ── Pass 1: G-Buffer fill (opaque only) ──
	_gBuffer->Clear();
	_gBuffer->BindAsTarget();

	RenderContext gbufCtx;
	gbufCtx.view = V;
	gbufCtx.proj = P;
	gbufCtx.lightArray = lightArray;
	gbufCtx.deferredPass = true;
	GET_SINGLE(InstancingManager)->Render(gbufCtx, _vecOpaque);

	// ── Pass 2: Deferred lighting (fullscreen) ──
	GRAPHICS->RestoreMainRenderTarget();

	auto lightingShader = RESOURCES->Get<HlslShader>(L"DeferredLighting_HLSL");
	if (lightingShader)
	{
		RENDER_STATES->BindAllSamplersPS();
		_gBuffer->BindSRVsPS(0);

		if (auto mat = RESOURCES->Get<Material>(L"DefaultMaterial"))
			if (mat->GetShadowMap())
				lightingShader->SetPSSRV(3, mat->GetShadowMap()->GetComPtr().Get());

		lightingShader->Bind();
		lightingShader->PushGlobalData(V, P);
		lightingShader->PushLightArrayData(*lightArray);

		MaterialDesc defaultMat;
		defaultMat.ambient  = Vec4(1.f);
		defaultMat.diffuse  = Vec4(1.f);
		defaultMat.specular = Vec4(1.f, 1.f, 1.f, 32.f);
		lightingShader->PushMaterialData(defaultMat);

		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->Draw(3, 0);

		_gBuffer->UnbindSRVsPS(0);
	}

	// ── Pass 3: Forward pass (skybox + transparent) using G-Buffer depth ──
	ID3D11RenderTargetView* mainRTV = GRAPHICS->GetRTV().Get();
	DCT->OMSetRenderTargets(1, &mainRTV, _gBuffer->GetDSV().Get());

	RenderContext fwdCtx;
	fwdCtx.view = V;
	fwdCtx.proj = P;
	fwdCtx.lightArray = lightArray;
	GET_SINGLE(InstancingManager)->Render(fwdCtx, _vecBackground);
	GET_SINGLE(InstancingManager)->Render(fwdCtx, _vecTransparent);

	// ── G-Buffer Debug View (KEY_3 toggle) ──
	if (INPUT->GetButtonDown(KEY_TYPE::KEY_3))
		_showGBufferDebug = !_showGBufferDebug;

	if (_showGBufferDebug)
	{
		auto debugShader = RESOURCES->Get<HlslShader>(L"GBufferDebug_HLSL");
		if (debugShader)
		{
			RENDER_STATES->BindAllSamplersPS();
			_gBuffer->BindSRVsPS(0);

			debugShader->Bind();
			debugShader->PushGlobalData(V, P);

			DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
			DCT->Draw(3, 0);
			DCT->OMSetDepthStencilState(nullptr, 0);

			_gBuffer->UnbindSRVsPS(0);
		}
	}
}

Camera::Camera() : Super(ComponentType::Camera)
{
	SceneWindowDesc sceneDesc;
	_width = static_cast<float>(sceneDesc.size.x);
	_height = static_cast<float>(sceneDesc.size.y);
}

Camera::~Camera()
{

}

void Camera::Update()
{
	UpdateMatrix();
}

void Camera::UpdateMatrix()
{
	Vec3 eyePosition = GetTransform()->GetPosition();
	Vec3 focusPosition = eyePosition + GetTransform()->GetLook();
	Vec3 upDirection = GetTransform()->GetUp();
	_matView = ::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

	if (_type == ProjectionType::Perspective)
	{
		_matProjection = ::XMMatrixPerspectiveFovLH(_fov, _width / _height, _near, _far);
	}
	else
	{
		_matProjection = ::XMMatrixOrthographicLH(_width, _height, _near, _far);
	}
}
