#include "pch.h"
#include "ImGuiManager.h"
#include "GameObject.h"


void ImGuiManager::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(GAME->GetGameDesc().hWnd);
	ImGui_ImplDX11_Init(DEVICE.Get(), DC.Get());
}

void ImGuiManager::Update()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();	
}

void ImGuiManager::Render()
{
	// Rendering
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::CreateEmptyGameObject()
{
	shared_ptr<GameObject> obj = make_shared<GameObject>();

	wstring name = L"GameObject " + to_wstring(obj->GetId());
	obj->SetObjectName(name);
	obj->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, 0.f });
	CUR_SCENE->Add(obj);
}

void ImGuiManager::RemoveGameObject(int32 id)
{
	if(id == -1)
		return;
	
	shared_ptr<GameObject> obj = CUR_SCENE->GetCreatedObject(id);

	if(obj != nullptr)
		CUR_SCENE->Remove(obj);
}
