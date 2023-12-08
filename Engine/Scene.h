#pragma once
class Scene
{
public:
	virtual void Start();
	virtual void Update();
	virtual void LateUpdate();
	
	virtual void Render();

	virtual void Add(shared_ptr<GameObject> object);
	virtual void Remove(shared_ptr<GameObject> object);

	unordered_set<shared_ptr<GameObject>>& GetObjects() { return _objects; }
	map<int64, shared_ptr<GameObject>>& GetCreatedObjects() { return _createdObjectsById; }
	shared_ptr<GameObject> GetCreatedObject(int32 id) 
	{
		if(_createdObjectsById.find(id) != _createdObjectsById.end()) 
			return  _createdObjectsById[id];

		return nullptr;
	}

	shared_ptr<GameObject> FindCreatedObjectByName(wstring name) {
		auto it = _createdObjectsByName.find(name);
		if (it != _createdObjectsByName.end()) {
			return it->second;
		}
		else {
			return nullptr; 
		}
	}

	shared_ptr<GameObject> GetMainCamera();
	shared_ptr<GameObject> GetUICamera();
	shared_ptr<GameObject> GetLight() { return _lights.empty() ? nullptr : *_lights.begin(); }

	void PickUI();
	shared_ptr<class GameObject> Pick(int32 screenX, int32 screenY);
	shared_ptr<class GameObject> MeshPick(int32 screenX, int32 screenY);

	void UnPickAll();

	void CheckCollision();

private:
	unordered_set<shared_ptr<GameObject>> _objects;
	// Cache Camera
	unordered_set<shared_ptr<GameObject>> _cameras;
	// Cache Light
	unordered_set<shared_ptr<GameObject>> _lights;
	
	// Cache Sorted by Time 
	map<int64, shared_ptr<GameObject>> _createdObjectsById;

	// Cache Sorted by Name 
	map<wstring, shared_ptr<GameObject>> _createdObjectsByName;
};

