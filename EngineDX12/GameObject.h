#pragma once
#include "Component.h"

// DX11 Engine/GameObject.h 이식 — 컴포넌트 컨테이너 + 계층 루트.
// 1차 이식: Transform + 제네릭 GetComponent<T>/AddComponent + 에디터 메타데이터.
// (Camera/Light 등 구체 컴포넌트 접근자는 해당 컴포넌트 포팅 시 추가)
class Transform;
class Renderer;
class Camera;
class Light;
class Terrain;
class MeshRenderer;
class ModelAnimator;
class MonoBehaviour;

class GameObject : public enable_shared_from_this<GameObject>
{
public:
	GameObject();
	~GameObject() {}

	void Awake();
	void Start();
	void Update();
	void LateUpdate();
	void FixedUpdate();

	template<typename T>
	shared_ptr<T> GetComponent()
	{
		for (int i = 0; i < FIXED_COMPONENT_COUNT; i++)
		{
			if (_components[i])
				if (auto casted = dynamic_pointer_cast<T>(_components[i]))
					return casted;
		}
		for (const auto& s : _scripts)
			if (auto casted = dynamic_pointer_cast<T>(s))
				return casted;
		return nullptr;
	}

	const vector<shared_ptr<MonoBehaviour>>& GetMonoBehaviours() { return _scripts; }

	shared_ptr<Component> GetFixedComponent(ComponentType type);
	shared_ptr<Transform> GetTransform();
	shared_ptr<Renderer>  GetRenderer();
	shared_ptr<Camera>    GetCamera();
	shared_ptr<Light>     GetLight();
	shared_ptr<Terrain>   GetTerrain();
	shared_ptr<MeshRenderer>  GetMeshRenderer();
	shared_ptr<ModelAnimator> GetModelAnimator();
	shared_ptr<Transform> GetOrAddTransform();

	void AddComponent(shared_ptr<Component> component);
	void RemoveComponent(ComponentType type);
	void RemoveScript(const shared_ptr<MonoBehaviour>& s);

	// ── 식별/정렬 ──
	const int64 GetCreatedTime() { return _createdTime; }
	const int64 GetId() { return _id; }
	bool operator<(const GameObject& b)
	{
		if (_createdTime == b._createdTime) return _id < b._id;
		return _createdTime < b._createdTime;
	}

	// ── 이름/레이어 ──
	void    SetObjectName(const wstring& name) { if (!name.empty()) _name = name; }
	wstring GetObjectName() { return _name; }
	void    SetLayerIndex(uint8 layer) { _layerIndex = layer; }
	uint8   GetLayerIndex() { return _layerIndex; }

	// ── 에디터 메타데이터 ──
	void SetUIPickable(bool on) { _pickable = on; }
	bool GetUIPickable() { return _pickable; }
	void SetUIPicked(bool on) { _picked = on; }
	bool GetUIPicked() { return _picked; }
	void SetEditorInternal(bool on) { _editorInternal = on; }
	bool IsEditorInternal() { return _editorInternal; }
	void SetEnableOutline(bool on) { _isOutlined = on; }
	bool GetEnableOutline() { return _isOutlined; }
	void SetActive(bool on) { _active = on; }
	bool IsActive() { return _active; }

protected:
	array<shared_ptr<Component>, FIXED_COMPONENT_COUNT> _components;
	vector<shared_ptr<MonoBehaviour>>                   _scripts;

	uint8   _layerIndex = 0;
	wstring _name;
	bool    _editorInternal = false;

private:
	int64          _createdTime = -1;
	static uint64  _nextId;
	uint64         _id = 0;

	bool _pickable = true;
	bool _picked = false;
	bool _isOutlined = true;
	bool _active = true; // 비활성 시 렌더 스킵
};
