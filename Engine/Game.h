#pragma once

// 기즈모 조작 모드 — 비트값은 ImGuizmo::OPERATION 과 동일 (캐스팅 호환)
enum OPERATION
{
	TRANSLATE_X = (1u << 0),
	TRANSLATE_Y = (1u << 1),
	TRANSLATE_Z = (1u << 2),
	ROTATE_X = (1u << 3),
	ROTATE_Y = (1u << 4),
	ROTATE_Z = (1u << 5),
	ROTATE_SCREEN = (1u << 6),
	SCALE_X = (1u << 7),
	SCALE_Y = (1u << 8),
	SCALE_Z = (1u << 9),
	BOUNDS = (1u << 10),
	SCALE_XU = (1u << 11),
	SCALE_YU = (1u << 12),
	SCALE_ZU = (1u << 13),

	TRANSLATE = TRANSLATE_X | TRANSLATE_Y | TRANSLATE_Z,
	ROTATE = ROTATE_X | ROTATE_Y | ROTATE_Z | ROTATE_SCREEN,
	SCALE = SCALE_X | SCALE_Y | SCALE_Z,
	SCALEU = SCALE_XU | SCALE_YU | SCALE_ZU, // universal
	UNIVERSAL = TRANSLATE | ROTATE | SCALEU
};

struct SceneDesc
{
	ImDrawList* drawList; // 씬 윈도우 드로우리스트 (ImGuizmo 가 여기에 그림)

	float x = 0.f;
	float y = 21.f;
	float width = 800.f;
	float height = 530.f;
};


struct GameDesc
{
	shared_ptr<class IExecute> app = nullptr;
	wstring appName = L"GameCoding";
	HINSTANCE hInstance = 0;
	HWND hWnd = 0;
	float width = 1600;
	float height = 900;
	bool vsync = false;
	bool windowed = true;
	
	float sceneWidth = 800;
	float sceneHeight = 600;

	Color clearColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
};

class Game
{
	DECLARE_SINGLE(Game);
public:
	
	WPARAM Run(GameDesc& gameDesc, SceneDesc& sceneDesc);
	LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);

	GameDesc& GetGameDesc() { return _gameDesc; }
	SceneDesc& GetSceneDesc() { return _sceneDesc; }

private:
	ATOM MyRegisterClass();
	BOOL InitInstance(int cmdShow);

	void Update();
	void ShowFps();

private:
	GameDesc _gameDesc;
	SceneDesc _sceneDesc;


};

