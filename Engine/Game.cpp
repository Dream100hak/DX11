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


WPARAM Game::Run(GameDesc& desc)
{

	_desc = desc;
	assert(_desc.app != nullptr);

	// 1) 윈도우 창 정보 등록
	MyRegisterClass();

	// 2) 윈도우 창 생성
	if (!InitInstance(SW_SHOWNORMAL))
		return FALSE;

	GRAPHICS->Init(_desc.hWnd);
	TIME->Init();
	INPUT->Init(_desc.hWnd);
	GUI->Init();
	RESOURCES->Init();
	
	_desc.app->Init();

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
	wcex.hInstance = _desc.hInstance;
	wcex.hIcon = ::LoadIcon(NULL, IDI_WINLOGO);
	wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = _desc.appName.c_str();
	wcex.hIconSm = wcex.hIcon;

	return RegisterClassExW(&wcex);
}

BOOL Game::InitInstance(int cmdShow)
{
	RECT windowRect = { 0, 0, _desc.width, _desc.height };
	::AdjustWindowRect(&windowRect, WS_POPUP, false);

	_desc.hWnd = CreateWindowW(_desc.appName.c_str(), _desc.appName.c_str(), WS_POPUP,
		CW_USEDEFAULT, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, _desc.hInstance, nullptr);

	if (!_desc.hWnd)
		return FALSE;

	::ShowWindow(_desc.hWnd, cmdShow);
	::UpdateWindow(_desc.hWnd);

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
	//case WM_LBUTTONDOWN:
	//case WM_MBUTTONDOWN:
	//case WM_RBUTTONDOWN:
	//	_desc.app->OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	//	break;
	//case WM_LBUTTONUP:
	//case WM_MBUTTONUP:
	//case WM_RBUTTONUP:
	//	_desc.app->OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	//	break;

	//case WM_MOUSEMOVE:
	//	_desc.app->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	//	break;
	case WM_MOUSEWHEEL:

		wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		scrollAmount = wheelDelta / 120;

		_desc.app->OnMouseWheel(scrollAmount);
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

	GRAPHICS->RenderBegin();

	SCENE->Update();

	GUI->Update();
	_desc.app->Update();
	_desc.app->Render();
	GUI->Render();

	GRAPHICS->RenderEnd();
}

void Game::ShowFps()
{
	uint32 fps = GET_SINGLE(TimeManager)->GetFps();

	WCHAR text[100] = L"";
	::wsprintf(text, L"FPS : %d", fps);

	::SetWindowText(_desc.hWnd, text);

}

