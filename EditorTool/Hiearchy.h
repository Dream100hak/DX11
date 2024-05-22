#pragma once
#include "EditorWindow.h"

class Hiearchy : public EditorWindow
{
public:
	Hiearchy(Vec2 pos, Vec2 size);
	virtual ~Hiearchy();

	virtual void Init() override;
	virtual void Update() override;

	void ShowHiearchy();


public:
	
	int32 CreateFire();
	int32 CreateRain();
	int32 CreateSky();
	int32 CreateTerrain();

};

