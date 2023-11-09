#include "pch.h"
#include "EditorToolManager.h"
#include "EditorWindow.h"

#include "MainMenuBar.h"
#include "GameEditorWindow.h"
#include "Hiearchy.h"
#include "Inspector.h"
#include "Project.h"
#include "LogWindow.h"

#include "Utils.h"


void EditorToolManager::Init()
{
	auto menuBar = make_shared<MainMenuBar>();
//	auto gameWnd = make_shared<GameEditorWindow>();
	auto hiearchy = make_shared<Hiearchy>();
	auto inspector = make_shared<Inspector>();
	auto project = make_shared<Project>();
	auto log = make_shared<LogWindow>();

	_editorWindows.insert({ Utils::GetPtrName(menuBar) , menuBar});
	//_editorWindows.insert({ Utils::GetPtrName(gameWnd) , gameWnd });
	_editorWindows.insert({ Utils::GetPtrName(hiearchy) , hiearchy });
	_editorWindows.insert({ Utils::GetPtrName(inspector) , inspector });
	_editorWindows.insert({ Utils::GetPtrName(project) , project });
	_editorWindows.insert({ Utils::GetPtrName(log) , log });

	for (auto wnd : _editorWindows)
	{
		if (wnd.second == nullptr)
			continue;

		wnd.second->Init();
	}
}

void EditorToolManager::Update()
{
	for (auto wnd : _editorWindows)
	{
		if (wnd.second == nullptr)
			continue;

		wnd.second->Update();
	}
}

std::shared_ptr<LogWindow> EditorToolManager::GetLog()
{
	{ return static_pointer_cast<LogWindow>(_editorWindows[Utils::GetClassNameEX<LogWindow>()]); }
}
