#include "pch.h"
#include "Main.h"
#include "Engine/Game.h"
#include "EditorTool.h"
#include "SceneDemo.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	{

		GameDesc gameDesc;
		gameDesc.appName = L"Editor";
		gameDesc.hInstance = hInstance;
		gameDesc.vsync = false;
		gameDesc.hWnd = NULL;
		gameDesc.width = 1920;
		gameDesc.height = 1080;
		gameDesc.clearColor = Color(0.f, 0.f, 0.f, 0.f);
		gameDesc.app = make_shared<EditorTool>();
		
		SceneDesc sceneDesc;
		sceneDesc.x = 0;
		sceneDesc.y = 21;
		sceneDesc.width = 800;
		sceneDesc.height = 530; 

		GAME->Run(gameDesc , sceneDesc);

	}
	return 0;
}