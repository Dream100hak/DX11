#include "pch.h"
#include "ImGuiManager.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Material.h"


void ImGuiManager::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;     

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	ImGui_ImplWin32_Init(GAME->GetGameDesc().hWnd);
	ImGui_ImplDX11_Init(DEVICE.Get(), DCT.Get());

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

int32 ImGuiManager::CreateEmptyGameObject()
{
	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();
	float distance = 10.0f; 
	Vec3 objectPosition = cam->GetLocalPosition() + (cam->GetLook() * distance);

	shared_ptr<GameObject> obj = make_shared<GameObject>();

	wstring name = FindEmptyName(CreatedObjType::GAMEOBJ);
	obj->SetObjectName(name);
	obj->GetOrAddTransform()->SetPosition(objectPosition);
	obj->GetOrAddTransform()->SetLocalScale(Vec3{ 0.01f, 0.01f, 0.01f });
	CUR_SCENE->Add(obj);

	return obj->GetId();
}

void ImGuiManager::RemoveGameObject(int32 id)
{
	if(id == -1)
		return;
	
	shared_ptr<GameObject> obj = CUR_SCENE->GetCreatedObject(id);

	if(obj != nullptr)
		CUR_SCENE->Remove(obj);
}

wstring ImGuiManager::FindEmptyName(CreatedObjType type)
{
	string name = EnumToString<CreatedObjType>(type);
	wstring wNameBase = wstring(name.begin(), name.end());

	int32 cnt = 0;
	while (true)
	{
		wstring wName = wNameBase + L"_" + std::to_wstring(cnt); 
		shared_ptr<GameObject> obj = SCENE->GetCurrentScene()->FindCreatedObjectByName(wName);

		if (obj == nullptr)
			return wName;

		cnt++;
	}
}

int32 ImGuiManager::CreateMesh(CreatedObjType type)
{

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();
	float distance = 50.0f;
	Vec3 objectPosition = cam->GetLocalPosition() + (cam->GetLook() * distance);

	auto obj = make_shared<GameObject>();
	obj->GetOrAddTransform()->SetPosition(objectPosition);
	auto meshRenderer = make_shared<MeshRenderer>();

	shared_ptr<Mesh> mesh = make_shared<Mesh>();
	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();

	wstring name;

	switch (type)
	{
	case CreatedObjType::QUAD:
		mesh->CreateQuad();
		name = FindEmptyName(CreatedObjType::QUAD);
		break;
	case CreatedObjType::CUBE:
		mesh->CreateCube();
		name = FindEmptyName(CreatedObjType::CUBE);
		break;
	case CreatedObjType::SPHERE:
		mesh->CreateSphere();
		name = FindEmptyName(CreatedObjType::SPHERE);
		break;

	default:
		break;
	}

	obj->SetObjectName(name);

	meshRenderer->SetMesh(mesh);
	meshRenderer->SetMaterial(mat);
	meshRenderer->SetTechnique(2);
	obj->AddComponent(meshRenderer);
	obj->GetOrAddTransform()->SetLocalScale(Vec3{ 5.f, 5.f, 5.f });
	CUR_SCENE->Add(obj);

	return obj->GetId();

}
