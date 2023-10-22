#pragma once
#include "Component.h"

class MonoBehaviour : public Component
{
	using Super = Component;

public:
	MonoBehaviour();
	~MonoBehaviour();

	virtual void Awake() override;
	virtual void Update() override;

	wstring GetBehaviorName()
	{
		if (_name.empty())
		{
			_name = L"Cam Test";
		}
		return _name;
	}

private:
	wstring _name = L"";
};

