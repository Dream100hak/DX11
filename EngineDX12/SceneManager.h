#pragma once
#include "Common.h"
#include "Define.h"

class Scene;

// DX11 Engine/SceneManager 이식 — 현재 씬 보유 싱글톤. SCENE / CUR_SCENE 매크로로 접근.
class SceneManager
{
	DECLARE_SINGLE(SceneManager);

public:
	shared_ptr<Scene> GetCurrentScene() { return _currentScene; }
	void SetCurrentScene(shared_ptr<Scene> scene) { _currentScene = scene; }

private:
	shared_ptr<Scene> _currentScene;
};
