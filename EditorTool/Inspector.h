#pragma once
#include "EditorWindow.h"

class Inspector : public EditorWindow
{
public:
	Inspector();
	~Inspector();

	virtual void Init() override;
	virtual void Update() override;

	void ShowInspector();
};