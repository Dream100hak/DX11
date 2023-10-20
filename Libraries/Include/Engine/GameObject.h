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
	const int64 GetId() { return _id; }

	bool operator<(const GameObject& b)
	{
		if(_createdTime == b._createdTime)
			return _id < b._id;

		return _createdTime < b._createdTime;
	}

protected:
	array<shared_ptr<Component>, FIXED_COMPONENT_COUNT> _components;
	vector<shared_ptr<MonoBehaviour>> _scripts;

	uint8 _layerIndex = 0;
	wstring _name = L"";
public:

	std::string GetGUID() const {
		char buffer[40];
		snprintf(buffer, sizeof(buffer),
			"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
			_guid.Data1, _guid.Data2, _guid.Data3,
			_guid.Data4[0], _guid.Data4[1], _guid.Data4[2], _guid.Data4[3],
			_guid.Data4[4], _guid.Data4[5], _guid.Data4[6], _guid.Data4[7]);
		return std::string(buffer);
	}

private:
	GUID _guid; 

	int64 _createdTime = -1;
	static uint64 _nextId;
	uint64 _id = 0; 

};

