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
};

