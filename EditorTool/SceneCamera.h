#pragma once
#include "MonoBehaviour.h"

class SceneCamera : public MonoBehaviour
{
public:
	virtual void Start() override;
	virtual void Update() override;

	void RotateCam();
	void MoveCam(int32 scrollAmount);

private:
	
	float _speed = 10.f;

};

