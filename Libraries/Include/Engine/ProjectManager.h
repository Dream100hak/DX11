#pragma once
class ProjectManager
{
	DECLARE_SINGLE(ProjectManager);

public:
	void Init();
	void Update();

	virtual void Add(shared_ptr<GameObject> object);
	virtual void Remove(shared_ptr<GameObject> object);

private:
	
	unordered_set<shared_ptr<GameObject>> _objects;

};

