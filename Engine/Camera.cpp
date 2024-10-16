#include "pch.h"
#include "Camera.h"
#include "Scene.h"

void Camera::SortGameObject()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	_vecForward.clear();

	for (auto& gameObject : gameObjects)
	{
		if (IsCulled(gameObject->GetLayerIndex()))
			continue;

		if (gameObject->GetMeshRenderer() == nullptr
			&& gameObject->GetModelRenderer() == nullptr
			&& gameObject->GetModelAnimator() == nullptr)
			continue;

		_vecForward.push_back(gameObject);
	}
}
void Camera::Render_Forward()
{
	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	auto light = SCENE->GetCurrentScene()->GetLight()->GetLight();

	int32 tech = 0;

	if (INPUT->GetButton(KEY_TYPE::KEY_1))
		tech = TECH_WIREFRAME;
	if (INPUT->GetButton(KEY_TYPE::KEY_2))
		tech = TECH_CLOCKWISE;

	GET_SINGLE(InstancingManager)->Render(tech, nullptr, cam->GetViewMatrix() , cam->GetProjectionMatrix(), light, _vecForward);
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
