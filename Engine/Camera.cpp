#include "pch.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"

void Camera::SortGameObject()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	_vecForward.clear();
	_vecOpaque.clear();
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

		// ── Frustum Culling ──────────────────────────────────
		const BoundingBox& box = renderer->GetBoundingBox();
		bool hasValidBox = (box.Extents.x > 0.f || box.Extents.y > 0.f || box.Extents.z > 0.f);
		if (hasValidBox && !_frustum.IsInFrustum(box))
			continue;

		// ── RenderQueue 분류 ─────────────────────────────────
		RenderQueue queue = RenderQueue::Opaque;
		if (auto mr = gameObject->GetMeshRenderer())
			if (mr->GetMaterial()) queue = mr->GetMaterial()->GetRenderQueue();

		if (static_cast<int32>(queue) >= static_cast<int32>(RenderQueue::Transparent))
			_vecTransparent.push_back(gameObject);
		else
			_vecOpaque.push_back(gameObject);

		_vecForward.push_back(gameObject); // 레거시 호환
	}

	// ── 불투명: Front-to-Back 정렬 (Early-Z 활용) ────────────
	sort(_vecOpaque.begin(), _vecOpaque.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da < db;
		});

	// ── 투명: Back-to-Front 정렬 (알파 블렌딩 정확도) ─────────
	sort(_vecTransparent.begin(), _vecTransparent.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da > db; // 먼 것 먼저
		});
}

void Camera::Render_Forward()
{
	auto cam   = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	auto light = SCENE->GetCurrentScene()->GetLight()->GetLight();

	int32 tech = 0;
	if (INPUT->GetButton(KEY_TYPE::KEY_1)) tech = TECH_WIREFRAME;
	if (INPUT->GetButton(KEY_TYPE::KEY_2)) tech = TECH_CLOCKWISE;

	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();

	// 1) 불투명 오브젝트 (Front-to-Back, Depth Write ON)
	GET_SINGLE(InstancingManager)->Render(tech, nullptr, V, P, light, _vecOpaque);

	// 2) 반투명 오브젝트 (Back-to-Front, Depth Write OFF 권장)
	//  현재는 동일 패스로 렌더 ? BlendState는 Material/HlslShader 에서 각자 설정
	GET_SINGLE(InstancingManager)->Render(tech, nullptr, V, P, light, _vecTransparent);
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
