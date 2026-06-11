#include "pch.h"
#include "EditorToolManager.h"
#include "EditorWindow.h"
#include "SceneWindow.h"

#include "MainMenuBar.h"
#include "GameEditorWindow.h"
#include "SceneSerializer.h"
#include "LogWindow.h"
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

	// ── 레이아웃 (갭 없이 전체 화면 분할) ──
	// 상단행: Scene(렌더 파이프라인 결합으로 고정) | Hiearchy | Log | Inspector(우측 전체 높이)
	// 하단행: Project(폴더 트리) | FolderContents(와이드 에셋 브라우저)
	const float W = GAME->GetGameDesc().width;     // 1920
	const float H = GAME->GetGameDesc().height;    // 1080
	const float menuH = scenePos.y;                // 21 (메인 메뉴바)
	const float colW = 373.f;                      // Hiearchy/Log/Project 열 폭
	const float inspW = W - sceneSize.x - colW * 2;// Inspector 폭 (잔여 = 374)
	const float botY = menuH + sceneSize.y;        // 551 (하단행 시작)
	const float botH = H - botY;                   // 529

	auto sceneWnd = make_shared<SceneWindow>(scenePos , sceneSize);
	auto gameWnd = make_shared<GameEditorWindow>(scenePos, sceneSize); // Game 뷰 — 플레이 중 씬뷰 위에 표시
	auto menuBar = make_shared<MainMenuBar>();
	auto hiearchy = make_shared<Hiearchy>(Vec2(sceneSize.x, menuH), Vec2(colW, sceneSize.y));
	auto log = make_shared<LogWindow>(Vec2(sceneSize.x + colW, menuH), Vec2(colW, sceneSize.y));
	auto inspector = make_shared<Inspector>(Vec2(W - inspW, menuH), Vec2(inspW, H - menuH));
	auto project = make_shared<Project>(Vec2(0, botY), Vec2(colW, botH));
	auto folderContents = make_shared<FolderContents>(Vec2(colW, botY), Vec2(W - inspW - colW, botH));

	_editorWindows.insert({ Utils::GetPtrName(sceneWnd) , sceneWnd });
	_editorWindows.insert({ Utils::GetPtrName(gameWnd) , gameWnd });
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

namespace
{
	const wstring PLAY_SNAPSHOT_PATH = L"__play_snapshot.scene"; // 작업 디렉터리 임시 파일
}

void EditorToolManager::StartPlay()
{
	if (_isPlaying)
		return;

	// 씬 스냅샷 — Stop 시 이 상태로 복원 (플레이 중 기즈모/인스펙터 수정은 롤백됨)
	if (SceneSerializer::Save(PLAY_SNAPSHOT_PATH) == false)
		return;

	_isPlaying = true;
	ADDLOG("Play", LogFilter::Info);
}

void EditorToolManager::StopPlay()
{
	if (_isPlaying == false)
		return;

	SceneSerializer::Load(PLAY_SNAPSHOT_PATH);
	_isPlaying = false;
	ADDLOG("Stop - scene restored", LogFilter::Info);
}
