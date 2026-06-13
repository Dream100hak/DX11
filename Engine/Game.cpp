#include "pch.h"
#include "Game.h"
#include "IExecute.h"
#include <shellapi.h> // DragAcceptFiles / DragQueryFile (외부 파일 드롭)


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

	::DragAcceptFiles(_gameDesc.hWnd, TRUE); // 탐색기 등에서 파일 드롭 허용 (WM_DROPFILES)

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
		scrollAmount = wheelDelta / 60;

		_gameDesc.app->OnMouseWheel(scrollAmount);
		break;

	case WM_DROPFILES:
	{
		HDROP hDrop = reinterpret_cast<HDROP>(wParam);
		const UINT count = ::DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

		vector<wstring> paths;
		paths.reserve(count);
		for (UINT i = 0; i < count; ++i)
		{
			wchar_t buf[MAX_PATH] = L"";
			if (::DragQueryFileW(hDrop, i, buf, MAX_PATH) > 0)
				paths.push_back(buf);
		}
		::DragFinish(hDrop);

		if (_fileDropCallback && paths.empty() == false)
			_fileDropCallback(paths);
		break;
	}

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


	GRAPHICS->SetViewport(_sceneDesc.width , _sceneDesc.height , _sceneDesc.x , _sceneDesc.y);
	GRAPHICS->RenderBegin();   // ← 백버퍼를 렌더 타겟으로 설정
	
	// ? 주의: SCENE->Update()는 여기서 호출하지 않음
	// Scene은 SceneWindow에서 별도 렌더 타겟으로 렌더링됨
	
	GUI->Update();

	// Scene 창/SceneDesc 갱신은 에디터(SceneWindow)가 담당 — 씬은 RT 로 렌더되어 ImGui::Image 로 표시
	// (도킹 도입으로 백버퍼 패스스루 방식 폐기. SceneDesc 는 씬 이미지 영역 rect)
	_gameDesc.app->Update();
	_gameDesc.app->Render();

	GUI->Render();             // ← ImGui 렌더링 (백버퍼에)
	GRAPHICS->RenderEnd();     // ← Present()
}

void Game::ShowFps()
{
	uint32 fps = GET_SINGLE(TimeManager)->GetFps();

	WCHAR text[100] = L"";
	::wsprintf(text, L"FPS : %d", fps);

	::SetWindowText(_gameDesc.hWnd, text);

}
