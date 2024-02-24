#include "pch.h"
#include "EditorToolManager.h"
#include "EditorWindow.h"
#include "SceneWindow.h"

#include "MainMenuBar.h"
#include "GameEditorWindow.h"
#include "Hiearchy.h"
#include "Inspector.h"
#include "Project.h"
#include "FolderContents.h"
#include "LogWindow.h"

#include "Utils.h"


void EditorToolManager::Init()
{
	
	Vec2 scenePos(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
	Vec2 sceneSize(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height);

	auto sceneWnd = make_shared<SceneWindow>(scenePos , sceneSize);
	auto menuBar = make_shared<MainMenuBar>();
	auto hiearchy = make_shared<Hiearchy>(Vec2(800,51) , Vec2(373,500) );
	auto inspector = make_shared<Inspector>(Vec2(1546, 51), Vec2(373, 1010) );
	auto project = make_shared<Project>(Vec2(800, 551), Vec2(373, 500) );
	auto folderContents = make_shared<FolderContents>(Vec2(1173, 551), Vec2(373, 500) );
	auto log = make_shared<LogWindow>();

	_editorWindows.insert({ Utils::GetPtrName(sceneWnd) , sceneWnd });
	_editorWindows.insert({ Utils::GetPtrName(menuBar) , menuBar});

	_editorWindows.insert({ Utils::GetPtrName(hiearchy) , hiearchy });
	_editorWindows.insert({ Utils::GetPtrName(inspector) , inspector });
	_editorWindows.insert({ Utils::GetPtrName(project) , project });
	_editorWindows.insert({ Utils::GetPtrName(folderContents) , folderContents });
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
