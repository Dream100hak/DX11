#pragma once
#include "Common.h"

class GameObject;

// DX11 Engine/Scene.h 이식 — GameObject 컨테이너 + 생명주기 + 카메라/라이트/터레인 캐시.
// (Pick/CheckCollision/Render 는 Camera·Collider 컴포넌트 포팅 후 채움)
class Scene
{
public:
	virtual ~Scene() {}

	virtual void Start();
	virtual void Update();
	virtual void LateUpdate();
	virtual void Render() {}   // TODO: GetMainCamera()->Render_Deferred() (Camera 포팅 후)

	virtual void Add(shared_ptr<GameObject> object);
	virtual void Remove(shared_ptr<GameObject> object);

	unordered_set<shared_ptr<GameObject>>& GetObjects() { return _objects; }
	map<int64, shared_ptr<GameObject>>&    GetCreatedObjects() { return _createdObjectsById; }

	shared_ptr<GameObject> GetCreatedObject(int64 id)
	{
		auto it = _createdObjectsById.find(id);
		return it != _createdObjectsById.end() ? it->second : nullptr;
	}
	shared_ptr<GameObject> FindCreatedObjectByName(const wstring& name)
	{
		auto it = _createdObjectsByName.find(name);
		return it != _createdObjectsByName.end() ? it->second : nullptr;
	}
	void RegisterName(shared_ptr<GameObject> object); // 이름 변경/복제 후 재등록 (Awake/Start 재실행 없이)

	shared_ptr<GameObject> GetMainCamera();
	shared_ptr<GameObject> GetLight()   { return _lights.empty()   ? nullptr : *_lights.begin(); }
	shared_ptr<GameObject> GetTerrain() { return _terrains.empty() ? nullptr : *_terrains.begin(); }
	unordered_set<shared_ptr<GameObject>>& GetLights() { return _lights; }

private:
	unordered_set<shared_ptr<GameObject>> _objects;
	unordered_set<shared_ptr<GameObject>> _cameras;
	unordered_set<shared_ptr<GameObject>> _lights;
	unordered_set<shared_ptr<GameObject>> _terrains;
	map<int64, shared_ptr<GameObject>>    _createdObjectsById;
	map<wstring, shared_ptr<GameObject>>  _createdObjectsByName;
};
