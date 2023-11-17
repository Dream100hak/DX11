#include "pch.h"
#include "Game.h"
#include "IExecute.h"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return GAME->WndProc(hwnd, msg, wParam, lParam);
}

WPARAM Game::Run(GameDesc& gameDesc , SceneDesc& sceneDesc)
{

	_gameDesc = gameDesc;
	assert(_gameDesc.app != nullptr);
	_sceneDesc = sceneDesc;

	// 1) 윈도우 창 정보 등록
	MyRegisterClass();

	// 2) 윈도우 창 생성
	if (!InitInstance(SW_SHOWNORMAL))
		return FALSE;
	
	GRAPHICS->Init(_gameDesc.hWnd);
	TIME->Init();
	INPUT->Init(_gameDesc.hWnd);
	GUI->Init();
	RESOURCES->Init();

	_gameDesc.app->Init();

	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		else
		{
			Update();
		}
	}

	return msg.wParam;
}


ATOM Game::MyRegisterClass()
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = _gameDesc.hInstance;
	wcex.hIcon = ::LoadIcon(NULL, IDI_WINLOGO);
	wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = _gameDesc.appName.c_str();
	wcex.hIconSm = wcex.hIcon;

	return RegisterClassExW(&wcex);
}

BOOL Game::InitInstance(int cmdShow)
{
	RECT windowRect = { 0, 0, _gameDesc.width, _gameDesc.height };
	::AdjustWindowRect(&windowRect, WS_POPUP, false);

	_gameDesc.hWnd = CreateWindowW(_gameDesc.appName.c_str(), _gameDesc.appName.c_str(), WS_POPUP,
		CW_USEDEFAULT, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, _gameDesc.hInstance, nullptr);

	if (!_gameDesc.hWnd)
		return FALSE;

	::ShowWindow(_gameDesc.hWnd, cmdShow);
	::UpdateWindow(_gameDesc.hWnd);

	return TRUE;
}

LRESULT CALLBACK Game::WndProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(handle, message, wParam, lParam))
		return true;

	int32 wheelDelta = 0;
	int32 scrollAmount = 0;

	switch (message)
	{
	case WM_SIZE:
		break;
	case WM_MOUSEWHEEL:

		wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		scrollAmount = wheelDelta / 120;

		_gameDesc.app->OnMouseWheel(scrollAmount);
		break;

	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProc(handle, message, wParam, lParam);
	}
}

void Game::Update()
{
	TIME->Update();
	INPUT->Update();
	ShowFps();

	//TODO : 여기다가 그림자 렌더링 과정

	GRAPHICS->SetViewport(_sceneDesc.width , _sceneDesc.height , _sceneDesc.x , _sceneDesc.y);
	GRAPHICS->PreRenderBegin();
	GRAPHICS->RenderBegin();

	SCENE->Update();
	GUI->Update();

	ImGui::SetNextWindowPos(ImVec2(_sceneDesc.x, _sceneDesc.y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(_sceneDesc.width, _sceneDesc.height), ImGuiCond_Appearing);
	
	ImGui::Begin("Scene", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
	
	float w = (float)ImGui::GetWindowWidth();
	float h = (float)ImGui::GetWindowHeight();
	float x = ImGui::GetWindowPos().x;
	float y = ImGui::GetWindowPos().y;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	_sceneDesc.drawList = drawList;
	_sceneDesc.x = x;
	_sceneDesc.y = y;

	if (ImGui::IsWindowCollapsed())
	{
		_sceneDesc.width = 0;
		_sceneDesc.height = 0;
	}
	else
	{
		_sceneDesc.width = w;
		_sceneDesc.height = h;
	}


	_gameDesc.app->Update();
	_gameDesc.app->Render();
	
	ImGui::End();

	GUI->Render();

	GRAPHICS->RenderEnd();
}

void Game::ShowFps()
{
	uint32 fps = GET_SINGLE(TimeManager)->GetFps();

	WCHAR text[100] = L"";
	::wsprintf(text, L"FPS : %d", fps);

	::SetWindowText(_gameDesc.hWnd, text);

}

