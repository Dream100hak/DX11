#include "pch.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "BindShaderDesc.h"  // ? LightArrayDesc, DirectionalLightData 필요

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

	// ── 멀티 라이트 배열 수집 ──────────────────────────────────────
	auto lightArray = make_shared<LightArrayDesc>();
	lightArray->lightCount = 0;
	
	// Scene의 모든 라이트 객체 수집
	auto& lights = scene->GetLights();
	for (auto& lightObj : lights)
	{
		if (lightArray->lightCount >= MAX_LIGHTS) break;
		
		auto lightComponent = lightObj->GetLight();
		if (!lightComponent) continue;
		
		const LightDesc& lightDesc = lightComponent->GetLightDesc();
		DirectionalLightData& data = lightArray->lights[lightArray->lightCount];
		
		data.diffuse = lightDesc.diffuse;
		data.ambient = lightDesc.ambient;
		data.intensity = lightDesc.intensity;
		data.direction = lightDesc.direction;
		
		lightArray->lightCount++;
	}

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
