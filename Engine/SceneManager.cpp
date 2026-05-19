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

	_currentScene->Render();   // Camera::Render_Forward() → HLSL 오브젝트 메인 백버퍼에 렌더
}
