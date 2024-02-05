#include "pch.h"
#include "ProjectManager.h"

void ProjectManager::Init()
{
	
}

void ProjectManager::Update()
{
	
}

void ProjectManager::Add(shared_ptr<GameObject> object)
{
	_objects.insert(object);
	object->Awake();
	object->Start();
}

void ProjectManager::Remove(shared_ptr<GameObject> object)
{
	_objects.erase(object);
}
