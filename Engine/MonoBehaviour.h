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

	void SetBehaviorName(const std::wstring& name) { _name = name; }
	std::wstring GetBehaviorName() const { return _name; }

private:
	wstring _name = L"";
};

