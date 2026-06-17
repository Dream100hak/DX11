#include "Common.h"
#include "D3D12Device.h"
#include "imgui.h"

// imgui_impl_win32 메시지 핸들러 (백엔드 전역 함수)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static D3D12Device g_device;
static const UINT WIN_W = 1280;
static const UINT WIN_H = 720;

static std::wstring DxrTierString(D3D12_RAYTRACING_TIER tier)
{
	switch (tier)
	{
	case D3D12_RAYTRACING_TIER_1_1: return L"DXR Tier 1.1";
	case D3D12_RAYTRACING_TIER_1_0: return L"DXR Tier 1.0";
	default:                        return L"DXR NOT supported";
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
		return true;

	switch (msg)
	{
	case WM_KEYDOWN:
		if (wp == VK_ESCAPE) PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		// 최소화(SIZE_MINIMIZED)는 무시, 그 외 클라이언트 크기로 스왑체인 재생성
		if (wp != SIZE_MINIMIZED)
			g_device.OnResize((UINT)LOWORD(lp), (UINT)HIWORD(lp));
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
	const wchar_t* kClass = L"EngineDX12Window";
	WNDCLASSEX wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClass;
	RegisterClassEx(&wc);

	RECT rc{ 0, 0, (LONG)WIN_W, (LONG)WIN_H };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	const int winW = rc.right - rc.left;
	const int winH = rc.bottom - rc.top;

	// 가장 오른쪽 모니터를 찾아 그 작업영역 중앙에 띄운다 (듀얼모니터: 왼쪽은 풀스크린 다른 작업용)
	struct MonPick { RECT best{}; bool found = false; };
	MonPick pick;
	EnumDisplayMonitors(nullptr, nullptr,
		[](HMONITOR hMon, HDC, LPRECT, LPARAM data) -> BOOL
		{
			MONITORINFO mi{ sizeof(mi) };
			if (GetMonitorInfo(hMon, &mi))
			{
				auto* p = reinterpret_cast<MonPick*>(data);
				if (!p->found || mi.rcWork.left > p->best.left)
				{
					p->best = mi.rcWork;
					p->found = true;
				}
			}
			return TRUE;
		},
		reinterpret_cast<LPARAM>(&pick));

	int posX = CW_USEDEFAULT, posY = CW_USEDEFAULT;
	if (pick.found)
	{
		const int monW = pick.best.right - pick.best.left;
		const int monH = pick.best.bottom - pick.best.top;
		posX = pick.best.left + (monW - winW) / 2;
		posY = pick.best.top + (monH - winH) / 2;
	}

	HWND hwnd = CreateWindow(kClass, L"EngineDX12", WS_OVERLAPPEDWINDOW,
		posX, posY, winW, winH,
		nullptr, nullptr, hInstance, nullptr);

	try
	{
		g_device.Init(hwnd, WIN_W, WIN_H);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(hwnd, e.what(), "D3D12 Init Failed", MB_OK | MB_ICONERROR);
		return -1;
	}

	// 타이틀에 어댑터 + DXR 지원 표시 (Phase 0 검증 지표)
	std::wstring title = L"EngineDX12  |  " + g_device.GetAdapterName() + L"  |  " + DxrTierString(g_device.GetDXRTier());
	SetWindowText(hwnd, title.c_str());

	ShowWindow(hwnd, nCmdShow);

	MSG m{};
	while (m.message != WM_QUIT)
	{
		if (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&m);
			DispatchMessage(&m);
		}
		else
		{
			g_device.Render();
		}
	}

	g_device.Destroy();
	return 0;
}
