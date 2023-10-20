#include "pch.h"
#include "Shortcut.h"

void ShortcutManager::Init()
{

}

void ShortcutManager::Update()
{
	File();
	Menu();
	CreateEmpty();
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
		GUI->CreateEmptyGameObject();
}

void ShortcutManager::DeleteObject()
{
	//if (INPUT->GetButton(KEY_TYPE::CTRL) && INPUT->GetButton(KEY_TYPE::SHIFT) && INPUT->GetButtonDown(KEY_TYPE::B))
		//GUI->RemoveGameObject(EditorTool::_selectedObj);
}
