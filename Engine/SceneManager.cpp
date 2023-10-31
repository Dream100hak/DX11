#include "pch.h"
#include "SceneManager.h"

void SceneManager::Init()
{
	if (_currentScene == nullptr)
		return;

	_currentScene->Start();
}

void SceneManager::Update()
{
	if (_currentScene == nullptr)
		return;

	_currentScene->Update();
	_currentScene->LateUpdate();

	_currentScene->Render();
}
