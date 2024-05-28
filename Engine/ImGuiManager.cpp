#include "pch.h"
#include "ImGuiManager.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "ModelRenderer.h"

#include "Model.h"


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


int32 ImGuiManager::CreateEmptyGameObject(CreatedObjType type /*= CreatedObjType::GAMEOBJ*/)
{
	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();
	float distance = 10.0f;
	Vec3 pos = cam->GetLocalPosition() + (cam->GetLook() * distance);

	shared_ptr<GameObject> obj = make_shared<GameObject>();

	wstring name = FindEmptyName(type);
	obj->SetObjectName(name);
	obj->GetOrAddTransform()->SetPosition(pos);
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
	float distance = 10.0f;
	Vec3 objectPosition = cam->GetLocalPosition() + (cam->GetLook() * distance);

	auto obj = make_shared<GameObject>();
	obj->GetOrAddTransform()->SetPosition(objectPosition);
	auto meshRenderer = make_shared<MeshRenderer>();

	shared_ptr<Mesh> mesh = make_shared<Mesh>();
	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();
	mat->GetMaterialDesc().lightCount = 1;

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
	obj->GetOrAddTransform()->SetLocalScale(Vec3{ 1.f, 1.f, 1.f });
	CUR_SCENE->Add(obj);

	return obj->GetId();

}

int32 ImGuiManager::CreateModelMesh(shared_ptr<Model> model, Vec3 position /*= Vec3(0,0,0)*/)
{
	auto obj = make_shared<GameObject>();
	auto shader = RESOURCES->Get<Shader>(L"Standard");

	BoundingBox box = model->CalculateModelBoundingBox();
	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = (globalScale / modelScale) * 6;

	wstring name;
	name = FindEmptyName(CreatedObjType::MODEL);
	obj->SetObjectName(name);

	obj->GetOrAddTransform()->SetPosition(position);
	obj->GetOrAddTransform()->SetRotation(Vec3(0, 0, 0));
	obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	obj->AddComponent(make_shared<ModelRenderer>(shader));
	obj->GetModelRenderer()->SetModel(model);
	obj->GetModelRenderer()->SetPass(1);

	CUR_SCENE->Add(obj);

	return obj->GetId();
}
