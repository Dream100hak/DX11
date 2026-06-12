#include "pch.h"
#include "Scene.h"
#include "GameObject.h"
#include "BaseCollider.h"
#include "Camera.h"
#include "Terrain.h"
#include "Button.h"

#include "Model.h"
#include "ModelMesh.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"

#include "MathUtils.h"

void Scene::Start()
{
	unordered_set<shared_ptr<GameObject>> objects = _objects;

	for (shared_ptr<GameObject> object : objects)
	{
		object->Start();
	}
}

void Scene::Update()
{
	unordered_set<shared_ptr<GameObject>> objects = _objects;

	for (shared_ptr<GameObject> object : objects)
	{
		object->Update();
	}

	PickUI();
}

void Scene::LateUpdate()
{
	unordered_set<shared_ptr<GameObject>> objects = _objects;

	for (shared_ptr<GameObject> object : objects)
	{
		object->LateUpdate();
	}

	CheckCollision();
}

void Scene::Render()
{
	// 백버퍼(씬뷰)는 메인(에디터) 카메라만 — 전체 순회하면 마지막에 그린 카메라가
	// 씬뷰를 덮어쓴다 (게임 카메라 시점은 GameEditorWindow 가 자기 RT 로 별도 렌더)
	shared_ptr<GameObject> mainCamera = GetMainCamera();
	if (mainCamera == nullptr)
		return;

	mainCamera->GetCamera()->SortGameObject();
	mainCamera->GetCamera()->Render_Deferred();
}

void Scene::Add(shared_ptr<GameObject> object)
{
	_objects.insert(object);

	if (object->GetCamera() != nullptr)
		_cameras.insert(object);
	
	if (object->GetLight() != nullptr)
		_lights.insert(object);

	if (object->GetTerrain() != nullptr)
		_terrains.insert(object);
	
	_createdObjectsById[object->GetId()] = object;
	_createdObjectsByName[object->GetObjectName()] = object;

	object->Awake();
	object->Start();
}

void Scene::Remove(shared_ptr<GameObject> object)
{
	// 계층 정리 — 부모의 children 에서 분리, 자식은 월드 유지한 채 루트로 승격
	if (auto tr = object->GetTransform())
	{
		vector<shared_ptr<Transform>> children = tr->GetChildren(); // 승격 중 변형되므로 복사
		for (auto& child : children)
			child->SetParentKeepWorld(nullptr);

		if (auto parent = tr->GetParent())
			parent->RemoveChild(tr.get());
	}

	_objects.erase(object);
	_cameras.erase(object);
	_lights.erase(object);
	_terrains.erase(object);
	_createdObjectsById.erase(object->GetId());
	_createdObjectsByName.erase(object->GetObjectName());
}

std::shared_ptr<GameObject> Scene::GetMainCamera()
{
	// 에디터 카메라(editorInternal) 우선 — 씬에 게임 카메라를 배치해도 에디터 시점이 흔들리지 않게
	// (_cameras 는 포인터 정렬 set 이라 순회 순서가 비결정적)
	shared_ptr<GameObject> fallback = nullptr;
	for (auto& camera : _cameras)
	{
		if (camera->GetCamera()->GetProjectionType() != ProjectionType::Perspective)
			continue;

		if (camera->IsEditorInternal())
			return camera;

		if (fallback == nullptr)
			fallback = camera;
	}

	return fallback;
}

std::shared_ptr<GameObject> Scene::GetUICamera()
{
	for (auto& camera : _cameras)
	{
		if (camera->GetCamera()->GetProjectionType() == ProjectionType::Orthographic)
			return camera;
	}

	return nullptr;
}

void Scene::PickUI()
{
	if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON) == false)
		return;

	if (GetUICamera() == nullptr)
		return;

	POINT screenPt = INPUT->GetMousePos();

	shared_ptr<Camera> camera = GetUICamera()->GetCamera();

	const auto gameObjects = GetObjects();

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetButton() == nullptr)
			continue;

		if (gameObject->GetButton()->Picked(screenPt))
			gameObject->GetButton()->InvokeOnClicked();
	}
}

std::shared_ptr<class GameObject> Scene::Pick(int32 screenX, int32 screenY)
{
	shared_ptr<Camera> camera = GetMainCamera()->GetCamera();

	float width = GRAPHICS->GetViewport().GetWidth();
	float height = GRAPHICS->GetViewport().GetHeight();

	Matrix projectionMatrix = camera->GetProjectionMatrix();

	float viewX = (+2.0f * screenX / width - 1.0f) / projectionMatrix(0, 0);
	float viewY = (-2.0f * screenY / height + 1.0f) / projectionMatrix(1, 1);

	Matrix viewMatrix = camera->GetViewMatrix();
	Matrix viewMatrixInv = viewMatrix.Invert();

	const auto& gameObjects = GetObjects();

	float minDistance = FLT_MAX;
	shared_ptr<GameObject> picked;

	for (auto& gameObject : gameObjects)
	{
		if (camera->IsCulled(gameObject->GetLayerIndex()))
			continue;
		if (gameObject->GetCollider() == nullptr)
			continue;

		// ViewSpace에서 Ray 생성
		Vec4 rayOrigin = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Vec4 rayDir = Vec4(viewX, viewY, 1.0f, 0.0f);

		// WorldSpace에서 Ray 생성
		Vec3 worldRayOrigin = XMVector3TransformCoord(rayOrigin, viewMatrixInv);
		Vec3 worldRayDir = XMVector3TransformNormal(rayDir, viewMatrixInv);
		worldRayDir.Normalize();

		// WorldSpace에서 Ray 생성
		Ray ray = Ray(worldRayOrigin, worldRayDir);

		float distance = 0.f;
		if (gameObject->GetCollider()->Intersects(ray, OUT distance) == false)
			continue;

		if (distance < minDistance)
		{
			minDistance = distance;
			picked = gameObject;
		}
	}

	return picked;
}
std::shared_ptr<class GameObject> Scene::MeshPick(int32 screenX, int32 screenY)
{
	shared_ptr<Camera> camera = GetMainCamera()->GetCamera();

	float width = GRAPHICS->GetViewport().GetWidth();
	float height = GRAPHICS->GetViewport().GetHeight();

	Matrix projectionMatrix = camera->GetProjectionMatrix();

	float viewX = (+2.0f * screenX / width - 1.0f) / projectionMatrix(0, 0);
	float viewY = (-2.0f * screenY / height + 1.0f) / projectionMatrix(1, 1);

	Matrix viewMatrix = camera->GetViewMatrix();
	Matrix viewMatrixInv = viewMatrix.Invert();

	const auto& gameObjects = GetObjects();

	float minDistance = MathUtils::INF;

	shared_ptr<GameObject> picked;

	for (auto& gameObject : gameObjects)
	{
		if (camera->IsCulled(gameObject->GetLayerIndex()))
			continue;

		if (gameObject->GetRenderer() == nullptr)
			continue;

		if(gameObject->GetSkyBox() != nullptr)
			continue;

		Vec4 rayOrigin = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Vec4 rayDir = Vec4(viewX, viewY, 1.0f, 0.0f);

		Vec3 worldRayOrigin = XMVector3TransformCoord(rayOrigin, viewMatrixInv);
		Vec3 worldRayDir = XMVector3TransformNormal(rayDir, viewMatrixInv);
		worldRayDir.Normalize();

		Ray ray = Ray(worldRayOrigin, worldRayDir);

		Vec3 pickPos;
		float distance = 0.f;
	
		if (gameObject->GetRenderer()->Pick(screenX, screenY, OUT pickPos, OUT distance) == false)
			continue;

		if (distance < minDistance)
		{
			minDistance = distance;
			picked = gameObject;
		}
	}

	return picked;
}

void Scene::UnPickAll()
{
	const auto& gameObjects = GetObjects();

	for (auto& gameObject : gameObjects)
	{
		gameObject->SetUIPicked(false);
	}
}

void Scene::CheckCollision()
{
	vector<shared_ptr<BaseCollider>> colliders;

	for (shared_ptr<GameObject> object : _objects)
	{
		if (object->GetCollider() == nullptr)
			continue;

		colliders.push_back(object->GetCollider());
	}

	// BruteForce
	for (int32 i = 0; i < colliders.size(); i++)
	{
		for (int32 j = i + 1; j < colliders.size(); j++)
		{
			shared_ptr<BaseCollider>& other = colliders[j];
			if (colliders[i]->Intersects(other))
			{
				int a = 3;
			}
		}
	}
}