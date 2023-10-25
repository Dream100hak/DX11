#pragma once
#include "EditorWindow.h"

class GameEditorWindow : public EditorWindow
{
public:
	GameEditorWindow();
	~GameEditorWindow();

	virtual void Init() override;
	virtual void Update() override;

	void ShowGameWindow();
};
