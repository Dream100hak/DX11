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

	// 씬 렌더 직전 — 섀도우맵/SSAO 등 보조 패스를 같은 프레임 데이터로 그림
	if (_preRenderCallback)
		_preRenderCallback();

	_currentScene->Render();   // 메인(에디터) 카메라 → 백버퍼 디퍼드 렌더
}
