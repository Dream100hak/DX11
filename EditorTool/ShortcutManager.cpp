#include "pch.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"

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

}

void ShortcutManager::CreateEmpty()
{
	if(INPUT->GetButton(KEY_TYPE::CTRL) && INPUT->GetButton(KEY_TYPE::SHIFT) && INPUT->GetButtonDown(KEY_TYPE::B))
		TOOL->SetSelectedObjH(GUI->CreateEmptyGameObject());
}

void ShortcutManager::DeleteObject()
{
	if (INPUT->GetButtonDown(KEY_TYPE::DELETEX))
	{
		int32 id = SELECTED_H;
		if (id != -1)
		{
			TOOL->SetSelectedObjH(-1);
			GUI->RemoveGameObject(id);
		}
	}		
}
