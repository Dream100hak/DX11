#pragma once
#include "EditorWindow.h"

class Hiearchy : public EditorWindow
{
public:
	Hiearchy();
	~Hiearchy();

	virtual void Init() override;
	virtual void Update() override;

	void ShowHiearchy();
};

