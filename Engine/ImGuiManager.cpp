#include "pch.h"
#include "ImGuiManager.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Light.h"

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
	auto scene = SCENE->GetCurrentScene();
	auto cam = scene->GetMainCamera();
	
	if (!cam) return -1;

	auto obj = make_shared<GameObject>();
	Vec3 spawnPos = cam->GetTransform()->GetLocalPosition() + cam->GetTransform()->GetLook() * 10.f;
	obj->GetOrAddTransform()->SetPosition(spawnPos);
	auto meshRenderer = make_shared<MeshRenderer>();

	shared_ptr<Mesh> mesh = make_shared<Mesh>();
	
	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();
	mat->GetMaterialDesc().lightCount = MAX_LIGHTS;
	mat->SetRenderQueue(RenderQueue::Opaque);
	
	if (!mat->GetHlslShader())
	{
		auto shader = RESOURCES->Get<Shader>(L"Standard");
		if (shader)
			mat->SetShader(shader);
	}

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
	meshRenderer->SetTechnique(0);
	obj->AddComponent(meshRenderer);
	
	obj->GetOrAddTransform()->SetLocalScale(Vec3{ 1.f, 1.f, 1.f });
	
	CUR_SCENE->Add(obj);

	return obj->GetId();
}

int32 ImGuiManager::CreateLight(int32 lightType)
{
	auto cam = SCENE->GetCurrentScene()->GetMainCamera();
	if (!cam) return -1;

	Vec3 spawnPos = cam->GetTransform()->GetLocalPosition() + cam->GetTransform()->GetLook() * 10.f;

	auto obj = make_shared<GameObject>();
	obj->GetOrAddTransform()->SetPosition(spawnPos);

	auto light = make_shared<Light>();

	const char* names[] = { "DirectionalLight", "PointLight", "SpotLight" };
	wstring baseName = wstring(names[lightType], names[lightType] + strlen(names[lightType]));

	int32 cnt = 0;
	while (true)
	{
		wstring name = baseName + L"_" + std::to_wstring(cnt);
		if (!SCENE->GetCurrentScene()->FindCreatedObjectByName(name))
		{
			obj->SetObjectName(name);
			break;
		}
		cnt++;
	}

	LightType lt = static_cast<LightType>(lightType);

	LightDesc desc;
	desc.diffuse  = Color(1.f, 1.f, 1.f, 1.f);
	desc.ambient  = Color(0.2f, 0.2f, 0.2f, 1.f);
	desc.specular = Color(1.f, 1.f, 1.f, 1.f);
	desc.intensity = 1.f;

	if (lt == Directional)
		desc.direction = Vec3(0.f, -1.f, 1.f);
	else
		desc.direction = Vec3(0.f, -1.f, 0.f);

	light->SetLightType(lt);
	light->SetLightDesc(desc);
	light->SetIntensity(1.f);
	obj->AddComponent(light);

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

int32 ImGuiManager::CreateModelAnimatorMesh(shared_ptr<Model> model, Vec3 position /*= Vec3(0,0,0)*/, int32 animIndex /*= 0*/)
{
	auto obj = make_shared<GameObject>();
	auto shader = RESOURCES->Get<Shader>(L"Standard");

	BoundingBox box = model->CalculateModelBoundingBox();
	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = (globalScale / modelScale) * 6;

	obj->SetObjectName(FindEmptyName(CreatedObjType::MODEL));
	obj->GetOrAddTransform()->SetPosition(position);
	obj->GetOrAddTransform()->SetRotation(Vec3(0, 0, 0));
	obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	obj->AddComponent(make_shared<ModelAnimator>(shader));
	obj->GetModelAnimator()->SetModel(model);
	obj->GetModelAnimator()->SetPass(2);
	if (animIndex >= 0)
		obj->GetModelAnimator()->GetTweenDesc().curr.animIndex = animIndex;

	CUR_SCENE->Add(obj);

	return obj->GetId();
}
