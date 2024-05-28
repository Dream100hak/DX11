#pragma once
#include <boost/type_index.hpp>


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
	// ...
	Script,

	End,
};

BOOST_DESCRIBE_ENUM(ComponentType , Transform , Renderer, Camera, Animator, Light , Collider , Terrain, Button, BillBoard, SkyBox)

enum
{
	FIXED_COMPONENT_COUNT = static_cast<uint8>(ComponentType::End) - 1
};

class Component
{
public:
	Component(ComponentType type);
	virtual ~Component();

	virtual void OnInspectorGUI() {}


public:

	virtual void Awake() { }
	virtual void Start() { }
	virtual void Update() { }
	virtual void LateUpdate() { }
	virtual void FixedUpdate() { }

public:

	void HideInspectorInfo(bool on) { _hideInspectorInfo = on; }

public:
	
	ComponentType GetType() { return _type; }

	shared_ptr<GameObject> GetGameObject();
	shared_ptr<Transform> GetTransform();

	bool GetHideInspector() { return _hideInspectorInfo; }

private:
	friend class GameObject;
	void SetGameObject(shared_ptr<GameObject> gameObject) { _gameObject = gameObject; }

protected:
	ComponentType _type;
	weak_ptr<GameObject> _gameObject;

	bool _hideInspectorInfo = false;
};

