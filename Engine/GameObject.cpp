#include "pch.h"
#include "GameObject.h"
#include "MonoBehaviour.h"
#include "Transform.h"
#include "Camera.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Light.h"
#include "BaseCollider.h"
#include "Terrain.h"
#include "Button.h"
#include "Billboard.h"
#include "SkyBox.h"
#include <chrono>


uint64 GameObject::_nextId = 0;

GameObject::GameObject()
{
	_id = _nextId++;

	auto currentTime = std::chrono::system_clock::now();
	_createdTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
	HRESULT hr = CoCreateGuid(&_guid);
	CHECK(hr);
}

GameObject::~GameObject()
{

}

void GameObject::Awake()
{
	for (shared_ptr<Component>& component : _components)
	{
		if (component)
			component->Awake();
	}

	for (shared_ptr<MonoBehaviour>& script : _scripts)
	{
		script->Awake();
	}
}

void GameObject::Start()
{
	for (shared_ptr<Component>& component : _components)
	{
		if (component)
			component->Start();
	}

	for (shared_ptr<MonoBehaviour>& script : _scripts)
	{
		script->Start();
	}
}

void GameObject::Update()
{
	for (shared_ptr<Component>& component : _components)
	{
		if (component)
			component->Update();
	}

	for (shared_ptr<MonoBehaviour>& script : _scripts)
	{
		script->Update();
	}
}

void GameObject::LateUpdate()
{
	for (shared_ptr<Component>& component : _components)
	{
		if (component)
			component->LateUpdate();
	}

	for (shared_ptr<MonoBehaviour>& script : _scripts)
	{
		script->LateUpdate();
	}
}

void GameObject::FixedUpdate()
{
	for (shared_ptr<Component>& component : _components)
	{
		if (component)
			component->FixedUpdate();
	}

	for (shared_ptr<MonoBehaviour>& script : _scripts)
	{
		script->FixedUpdate();
	}
}

std::shared_ptr<Component> GameObject::GetFixedComponent(ComponentType type)
{
	uint8 index = static_cast<uint8>(type);
	assert(index < FIXED_COMPONENT_COUNT);
	return _components[index];
}

std::shared_ptr<Transform> GameObject::GetTransform()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Transform);
	return static_pointer_cast<Transform>(component);
}

std::shared_ptr<Camera> GameObject::GetCamera()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Camera);
	return static_pointer_cast<Camera>(component);
}

shared_ptr<Renderer> GameObject::GetRenderer()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Renderer);
	return static_pointer_cast<Renderer>(component);
}

// 렌더러는 ComponentType::Renderer 슬롯 하나를 공유하므로 실제 타입을 확인 후 캐스팅한다.
// (무확인 static_cast 는 다른 렌더러 타입일 때 잘못된 메모리를 읽는 UB — 크래시 원인이었음)
std::shared_ptr<MeshRenderer> GameObject::GetMeshRenderer()
{
	shared_ptr<Renderer> renderer = GetRenderer();
	if (renderer == nullptr || renderer->GetRenderType() != RendererType::Mesh)
		return nullptr;
	return static_pointer_cast<MeshRenderer>(renderer);
}

std::shared_ptr<ModelRenderer> GameObject::GetModelRenderer()
{
	shared_ptr<Renderer> renderer = GetRenderer();
	if (renderer == nullptr || renderer->GetRenderType() != RendererType::Model)
		return nullptr;
	return static_pointer_cast<ModelRenderer>(renderer);
}

std::shared_ptr<ModelAnimator> GameObject::GetModelAnimator()
{
	shared_ptr<Renderer> renderer = GetRenderer();
	if (renderer == nullptr || renderer->GetRenderType() != RendererType::Animator)
		return nullptr;
	return static_pointer_cast<ModelAnimator>(renderer);
}

std::shared_ptr<Light> GameObject::GetLight()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Light);
	return static_pointer_cast<Light>(component);
}

std::shared_ptr<BaseCollider> GameObject::GetCollider()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Collider);
	return static_pointer_cast<BaseCollider>(component);
}

std::shared_ptr<Terrain> GameObject::GetTerrain()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Terrain);
	return static_pointer_cast<Terrain>(component);
}

std::shared_ptr<Button> GameObject::GetButton()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Button);
	return static_pointer_cast<Button>(component);
}

std::shared_ptr<Billboard> GameObject::GetBillboard()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::BillBoard);
	return static_pointer_cast<Billboard>(component);
}

std::shared_ptr<SkyBox> GameObject::GetSkyBox()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::SkyBox);
	return static_pointer_cast<SkyBox>(component);
}

std::shared_ptr<Transform> GameObject::GetOrAddTransform()
{
	if (GetTransform() == nullptr)
	{
		shared_ptr<Transform> transform = make_shared<Transform>();
		AddComponent(transform);
	}

	return GetTransform();
}

void GameObject::AddComponent(shared_ptr<Component> component)
{
	component->SetGameObject(shared_from_this());

	uint8 index = static_cast<uint8>(component->GetType());
	if (index < FIXED_COMPONENT_COUNT)
	{
		_components[index] = component;
	}
	else
	{
		_scripts.push_back(dynamic_pointer_cast<MonoBehaviour>(component));
	}

	component->Awake();
	component->Start();
}