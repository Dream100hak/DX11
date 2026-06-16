#include "GameObject.h"
#include "Transform.h"
#include "Renderer.h"
#include "Camera.h"
#include "Light.h"
#include "Terrain.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "MonoBehaviour.h"
#include <chrono>

uint64 GameObject::_nextId = 0;

GameObject::GameObject()
{
	_id = _nextId++;
	_createdTime = (int64)std::chrono::steady_clock::now().time_since_epoch().count();
}

void GameObject::Awake()
{
	for (auto& c : _components) if (c) c->Awake();
	for (auto& s : _scripts) if (s) s->Awake();
}
void GameObject::Start()
{
	for (auto& c : _components) if (c) c->Start();
	for (auto& s : _scripts) if (s) s->Start();
}
void GameObject::Update()
{
	for (auto& c : _components) if (c) c->Update();
	for (auto& s : _scripts) if (s) s->Update();
}
void GameObject::LateUpdate()
{
	for (auto& c : _components) if (c) c->LateUpdate();
	for (auto& s : _scripts) if (s) s->LateUpdate();
}
void GameObject::FixedUpdate()
{
	for (auto& c : _components) if (c) c->FixedUpdate();
	for (auto& s : _scripts) if (s) s->FixedUpdate();
}

shared_ptr<Component> GameObject::GetFixedComponent(ComponentType type)
{
	uint8 idx = static_cast<uint8>(type);
	if (idx < FIXED_COMPONENT_COUNT) return _components[idx];
	return nullptr;
}

shared_ptr<Transform> GameObject::GetTransform()
{
	return static_pointer_cast<Transform>(GetFixedComponent(ComponentType::Transform));
}

shared_ptr<Renderer> GameObject::GetRenderer()
{
	return static_pointer_cast<Renderer>(GetFixedComponent(ComponentType::Renderer));
}

shared_ptr<Camera> GameObject::GetCamera()
{
	return static_pointer_cast<Camera>(GetFixedComponent(ComponentType::Camera));
}

shared_ptr<Light> GameObject::GetLight()
{
	return static_pointer_cast<Light>(GetFixedComponent(ComponentType::Light));
}

shared_ptr<Terrain> GameObject::GetTerrain()
{
	return static_pointer_cast<Terrain>(GetFixedComponent(ComponentType::Terrain));
}

shared_ptr<MeshRenderer> GameObject::GetMeshRenderer()
{
	return dynamic_pointer_cast<MeshRenderer>(GetFixedComponent(ComponentType::Renderer));
}

shared_ptr<ModelAnimator> GameObject::GetModelAnimator()
{
	return dynamic_pointer_cast<ModelAnimator>(GetFixedComponent(ComponentType::Renderer));
}

shared_ptr<Transform> GameObject::GetOrAddTransform()
{
	if (auto t = GetTransform()) return t;
	auto t = make_shared<Transform>();
	AddComponent(t);
	return t;
}

void GameObject::AddComponent(shared_ptr<Component> component)
{
	component->SetGameObject(shared_from_this());
	if (component->GetType() == ComponentType::Script)
		_scripts.push_back(static_pointer_cast<MonoBehaviour>(component));
	else
	{
		uint8 idx = static_cast<uint8>(component->GetType());
		if (idx < FIXED_COMPONENT_COUNT)
			_components[idx] = component;
	}
}

void GameObject::RemoveComponent(ComponentType type)
{
	if (type == ComponentType::Transform) return; // Transform 은 필수
	uint8 idx = static_cast<uint8>(type);
	if (idx < FIXED_COMPONENT_COUNT)
		_components[idx] = nullptr;
}
