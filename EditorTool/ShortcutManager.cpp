#include "pch.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "UndoManager.h"
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
	// Ctrl+Z / Ctrl+Y — Undo/Redo (플레이 중·위젯 편집 중 제외)
	if (TOOL->IsPlaying() || ImGui::IsAnyItemActive())
		return;

	if (INPUT->GetButton(KEY_TYPE::CTRL))
	{
		if (INPUT->GetButtonDown(KEY_TYPE::Z))
			UndoManager::Undo();
		if (INPUT->GetButtonDown(KEY_TYPE::Y))
			UndoManager::Redo();
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
