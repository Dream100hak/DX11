#pragma once
#include <boost/type_index.hpp>


class GameObject;
class Transform;

enum class ComponentType : uint8
{
	Transform,
	MeshRenderer,
	ModelRenderer,
	Camera,
	Animator,
	Light,
	Collider,
	Terrain,
	Button,
	BillBoard,
	SnowBillBoard,
	// ...
	Script,

	End,
};

BOOST_DESCRIBE_ENUM(ComponentType , Transform , MeshRenderer , ModelRenderer, Camera, Animator, Light , Collider , Terrain, Button, BillBoard, SnowBillBoard)

enum
{
	FIXED_COMPONENT_COUNT = static_cast<uint8>(ComponentType::End) - 1
};

class Component
{
public:
	Component(ComponentType type);
	virtual ~Component();

	virtual void OnInspectorGUI()
	{
		
	}

public:

	virtual void Awake() { }
	virtual void Start() { }
	virtual void Update() { }
	virtual void LateUpdate() { }
	virtual void FixedUpdate() { }

public:
	

	ComponentType GetType() { return _type; }

	shared_ptr<GameObject> GetGameObject();
	shared_ptr<Transform> GetTransform();

	template<typename T> void foo(const T a) // 실제 템플릿 타입 T와 변수타입은 다를 수 있음
	{
		cout << type_id_with_cvr<T>().pretty_name() << endl;
		cout << type_id_with_cvr<decltype(a)>().pretty_name() << endl;
	}


private:
	friend class GameObject;
	void SetGameObject(shared_ptr<GameObject> gameObject) { _gameObject = gameObject; }

protected:
	ComponentType _type;
	weak_ptr<GameObject> _gameObject;

};

