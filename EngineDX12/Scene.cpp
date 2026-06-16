#include "Scene.h"
#include "GameObject.h"
#include "Component.h"

void Scene::Start()
{
	for (auto& o : _objects) if (o) o->Start();
}
void Scene::Update()
{
	for (auto& o : _objects) if (o) o->Update();
}
void Scene::LateUpdate()
{
	for (auto& o : _objects) if (o) o->LateUpdate();
}

void Scene::Add(shared_ptr<GameObject> object)
{
	if (!object) return;
	_objects.insert(object);
	_createdObjectsById[object->GetId()] = object;
	_createdObjectsByName[object->GetObjectName()] = object;

	// 컴포넌트 종류별 캐시 (해당 컴포넌트가 붙어 있으면)
	if (object->GetFixedComponent(ComponentType::Camera))  _cameras.insert(object);
	if (object->GetFixedComponent(ComponentType::Light))   _lights.insert(object);
	if (object->GetFixedComponent(ComponentType::Terrain)) _terrains.insert(object);

	object->Awake();
	object->Start();
}

void Scene::Remove(shared_ptr<GameObject> object)
{
	if (!object) return;
	_objects.erase(object);
	_createdObjectsById.erase(object->GetId());
	_createdObjectsByName.erase(object->GetObjectName());
	_cameras.erase(object);
	_lights.erase(object);
	_terrains.erase(object);
}

void Scene::RegisterName(shared_ptr<GameObject> object)
{
	if (!object) return;
	_createdObjectsByName[object->GetObjectName()] = object;
}

shared_ptr<GameObject> Scene::GetMainCamera()
{
	return _cameras.empty() ? nullptr : *_cameras.begin();
}
