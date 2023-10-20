#include "pch.h"
#include "Main.h"
#include "Engine/Game.h"
#include "EditorTool.h"
#include "SceneDemo.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	{
		GameDesc desc;
		desc.appName = L"GameCoding";
		desc.hInstance = hInstance;
		desc.vsync = false;
		desc.hWnd = NULL;
		desc.width = 1980;
		desc.height = 1080;
		desc.clearColor = Color(0.f, 0.f, 0.f, 0.f);

		desc.app = make_shared<EditorTool>();

		GAME->Run(desc);
	}
	return 0;
}