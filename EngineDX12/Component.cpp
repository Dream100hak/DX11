#include "Component.h"
#include "GameObject.h"

shared_ptr<GameObject> Component::GetGameObject()
{
	return _gameObject.lock();
}

shared_ptr<Transform> Component::GetTransform()
{
	if (auto go = _gameObject.lock())
		return go->GetTransform();
	return nullptr;
}
