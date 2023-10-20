#pragma once
#include "Component.h"
class MonoBehaviour;
class Transform;
class Camera;
class MeshRenderer;
class ModelRenderer;
class ModelAnimator;
class Light;
class BaseCollider;
class Terrain;
class Button;
class Billboard;
class SnowBillboard;


class GameObject : public enable_shared_from_this<GameObject>
{
public:
	GameObject();
	~GameObject();

public:

	void Awake();
	void Start();
	void Update();
	void LateUpdate();
	void FixedUpdate();

	shared_ptr<Component> GetFixedComponent(ComponentType type);
	shared_ptr<Transform> GetTransform();
	shared_ptr<Camera> GetCamera();
	shared_ptr<MeshRenderer> GetMeshRenderer();
	shared_ptr<ModelRenderer> GetModelRenderer();
	shared_ptr<ModelAnimator> GetModelAnimator();
	shared_ptr<Light> GetLight();
	shared_ptr<BaseCollider> GetCollider();
	shared_ptr<Terrain> GetTerrain();
	shared_ptr<Button> GetButton();
	shared_ptr<Billboard> GetBillboard();
	shared_ptr<SnowBillboard> GetSnowBillboard();

	shared_ptr<Transform> GetOrAddTransform();
	void AddComponent(shared_ptr<Component> component);

	void SetLayerIndex(uint8 layer) { _layerIndex = layer; }
	uint8 GetLayerIndex() { return _layerIndex; }

	void SetObjectName(const wstring& name) { if (!name.empty()) _name.assign(name.begin(), name.end()); }
	wstring GetObjectName() { return _name; }

	const int64 GetCreatedTime() {return _createdTime; }

	bool operator<(const GameObject& b)
	{
		return _createdTime < b._createdTime;
	}

protected:
	array<shared_ptr<Component>, FIXED_COMPONENT_COUNT> _components;
	vector<shared_ptr<MonoBehaviour>> _scripts;

	uint8 _layerIndex = 0;
	wstring _name = L"";
	int64 _createdTime = -1;
};

