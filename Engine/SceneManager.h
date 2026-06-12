#pragma once
#include "Scene.h"

class SceneManager
{
	DECLARE_SINGLE(SceneManager);

public:
	
	void Init();
	void Update();

	template<typename T>
	void ChangeScene(shared_ptr<T> scene)
	{
		_currentScene = scene;
		scene->Start();
	}

	shared_ptr<Scene> GetCurrentScene() { return _currentScene; }

	// 씬 렌더 직전 훅 (오브젝트 업데이트 후) — 에디터가 섀도우맵/SSAO 를 같은 프레임
	// 데이터로 그리는 지점. 1회 등록 (구 잡큐의 매 프레임 재등록 + 1프레임 지연 대체)
	void SetPreRenderCallback(std::function<void()> callback) { _preRenderCallback = std::move(callback); }

private:
	shared_ptr<Scene> _currentScene = make_shared<Scene>();
	std::function<void()> _preRenderCallback;
};

