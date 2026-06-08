#pragma once
#include "MonoBehaviour.h"

class SkyCubeMap : public MonoBehaviour
{
	using Super = Component;

public:
	SkyCubeMap();
	virtual ~SkyCubeMap();

	void Init(wstring fileName);

private:

	shared_ptr<Texture> _cubeMap = nullptr;

};

