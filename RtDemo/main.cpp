#include "Common.h"
#include "D3D12Device.h"

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
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wp == VK_ESCAPE) PostQuitMessage(0);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
	const wchar_t* kClass = L"RtDemoWindow";
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

	HWND hwnd = CreateWindow(kClass, L"RtDemo (DX12)", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
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
	std::wstring title = L"RtDemo (DX12)  |  " + g_device.GetAdapterName() + L"  |  " + DxrTierString(g_device.GetDXRTier());
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
