#include "pch.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "UndoManager.h"
#include "SceneSerializer.h"
#include "LogWindow.h"

void ShortcutManager::Init()
{

}

void ShortcutManager::Update()
{
	File();
	Menu();
	CreateEmpty();
	DeleteObject();
}

void ShortcutManager::File()
{

}

void ShortcutManager::Menu()
{
	// 플레이 중·위젯 편집 중·우클릭 카메라 비행(WASD) 중에는 단축키 무시
	if (TOOL->IsPlaying() || ImGui::IsAnyItemActive() || INPUT->GetButton(KEY_TYPE::RBUTTON))
		return;

	if (INPUT->GetButton(KEY_TYPE::CTRL))
	{
		if (INPUT->GetButtonDown(KEY_TYPE::Z))
			UndoManager::Undo();
		if (INPUT->GetButtonDown(KEY_TYPE::Y))
			UndoManager::Redo();

		// Ctrl+D — 선택 오브젝트 복제 (자식 포함)
		if (INPUT->GetButtonDown(KEY_TYPE::D))
		{
			int32 id = SELECTED_H;
			if (id != -1 && CUR_SCENE->GetCreatedObject(id) != nullptr)
			{
				UndoManager::Record();
				int64 newId = SceneSerializer::Duplicate(id);
				if (newId != -1)
				{
					CUR_SCENE->UnPickAll();
					TOOL->SetSelectedObjH(newId);
					ADDLOG("Duplicate Object", LogFilter::Info);
				}
			}
		}
	}
}

void ShortcutManager::CreateEmpty()
{
	if (INPUT->GetButton(KEY_TYPE::CTRL) && INPUT->GetButton(KEY_TYPE::SHIFT) && INPUT->GetButtonDown(KEY_TYPE::B))
	{
		UndoManager::Record();
		TOOL->SetSelectedObjH(GUI->CreateEmptyGameObject());
		ADDLOG("Create GameObject", LogFilter::Info);
	}
}

void ShortcutManager::DeleteObject()
{
	if (INPUT->GetButtonDown(KEY_TYPE::DELETEX))
	{
		int32 id = SELECTED_H;
		if (id != -1)
		{
			UndoManager::Record();
			TOOL->SetSelectedObjH(-1);
			GUI->RemoveGameObject(id);
			ADDLOG("Remove GameObject", LogFilter::Warn);
		}
	}
}
