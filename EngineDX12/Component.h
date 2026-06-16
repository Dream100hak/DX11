#pragma once
#include "Common.h"

// DX11 Engine/Component.h 이식 — 컴포넌트 베이스 + 타입 enum.
class GameObject;
class Transform;

enum class ComponentType : uint8
{
	Transform,
	Renderer,
	Camera,
	Animator,
	Light,
	Collider,
	Terrain,
	Button,
	BillBoard,
	SkyBox,
	Script,

	End,
};

enum
{
	FIXED_COMPONENT_COUNT = static_cast<uint8>(ComponentType::End) - 1
};

class Component
{
public:
	Component(ComponentType type) : _type(type) {}
	virtual ~Component() {}

	virtual void OnInspectorGUI() {}

	virtual void Awake() {}
	virtual void Start() {}
	virtual void Update() {}
	virtual void LateUpdate() {}
	virtual void FixedUpdate() {}

	ComponentType                 GetType() { return _type; }
	shared_ptr<GameObject>        GetGameObject();
	shared_ptr<Transform>         GetTransform();

	void HideInspectorInfo(bool on) { _hideInspectorInfo = on; }
	bool GetHideInspector() { return _hideInspectorInfo; }

private:
	friend class GameObject;
	void SetGameObject(shared_ptr<GameObject> gameObject) { _gameObject = gameObject; }

protected:
	ComponentType         _type;
	weak_ptr<GameObject>  _gameObject;
	bool                  _hideInspectorInfo = false;
};
