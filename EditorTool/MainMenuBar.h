#pragma once
#include "EditorWindow.h"

class MainMenuBar : public EditorWindow
{
public:
	MainMenuBar();
	virtual ~MainMenuBar();

	virtual void Init() override;
	virtual void Update() override;

	void ShowMainMenuBar();
	void MenuFileList();
	void AppPlayMenu();

	// FBX 파일 선택 -> ufbx 변환 (.mesh/.mmat/.clip)
	void ConvertFbx();

};

