#include "pch.h"
#include "ImGuiManager.h"
#include "ImGuizmo.h"
#include <filesystem>
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

	// 폰트 — 기본 ProggyClean(13px 픽셀 폰트) 대신 맑은 고딕 16px + 한글 글리프
	{
		char winDir[MAX_PATH]{};
		::GetWindowsDirectoryA(winDir, MAX_PATH);
		string fontPath = string(winDir) + "\\Fonts\\malgun.ttf";

		if (std::filesystem::exists(fontPath))
			io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.f, nullptr, io.Fonts->GetGlyphRangesKorean());
	}

	ApplyEditorStyle();

	ImGui_ImplWin32_Init(GAME->GetGameDesc().hWnd);
	ImGui_ImplDX11_Init(DEVICE.Get(), DCT.Get());

}

// 에디터 전역 스타일 — 차콜 다크 + 블루 액센트 (기본 StyleColorsDark 는 채도 높고 각져서 구식 느낌)
void ImGuiManager::ApplyEditorStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();

	// 레이아웃 — 여백/라운딩
	style.WindowPadding     = ImVec2(10.f, 8.f);
	style.FramePadding      = ImVec2(8.f, 4.f);
	style.CellPadding       = ImVec2(6.f, 3.f);
	style.ItemSpacing       = ImVec2(8.f, 5.f);
	style.ItemInnerSpacing  = ImVec2(6.f, 4.f);
	style.IndentSpacing     = 16.f;
	style.ScrollbarSize     = 13.f;
	style.GrabMinSize       = 10.f;

	style.WindowBorderSize  = 1.f;
	style.ChildBorderSize   = 1.f;
	style.PopupBorderSize   = 1.f;
	style.FrameBorderSize   = 0.f;
	style.TabBorderSize     = 0.f;

	style.WindowRounding    = 6.f;
	style.ChildRounding     = 4.f;
	style.FrameRounding     = 3.f;
	style.PopupRounding     = 4.f;
	style.ScrollbarRounding = 9.f;
	style.GrabRounding      = 3.f;
	style.TabRounding       = 4.f;

	style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_None;

	const ImVec4 accent       = ImVec4(0.26f, 0.56f, 0.96f, 1.00f); // 블루 액센트
	const ImVec4 accentDim    = ImVec4(0.22f, 0.42f, 0.69f, 1.00f);
	const ImVec4 accentBright = ImVec4(0.36f, 0.65f, 1.00f, 1.00f);

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.93f, 1.00f);
	colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.51f, 0.53f, 1.00f);
	colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
	colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.11f, 0.12f, 0.98f);
	colors[ImGuiCol_Border]                = ImVec4(0.25f, 0.26f, 0.29f, 0.60f);
	colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.21f, 0.24f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.25f, 0.27f, 0.30f, 1.00f);
	colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.30f, 0.34f, 1.00f);
	colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgActive]         = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.11f, 0.12f, 0.75f);
	colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.11f, 0.12f, 0.13f, 0.60f);
	colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.38f, 0.42f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]   = accentDim;
	colors[ImGuiCol_CheckMark]             = accentBright;
	colors[ImGuiCol_SliderGrab]            = accentDim;
	colors[ImGuiCol_SliderGrabActive]      = accentBright;
	colors[ImGuiCol_Button]                = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);
	colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.30f, 0.35f, 1.00f);
	colors[ImGuiCol_ButtonActive]          = accentDim;
	colors[ImGuiCol_Header]                = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
	colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
	colors[ImGuiCol_HeaderActive]          = accentDim;
	colors[ImGuiCol_Separator]             = ImVec4(0.25f, 0.26f, 0.29f, 0.60f);
	colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.26f, 0.56f, 0.96f, 0.78f);
	colors[ImGuiCol_SeparatorActive]       = accent;
	colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.56f, 0.96f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.56f, 0.96f, 0.60f);
	colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.56f, 0.96f, 0.90f);
	colors[ImGuiCol_Tab]                   = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
	colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.42f, 0.66f, 1.00f);
	colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.30f, 0.45f, 1.00f);
	colors[ImGuiCol_TabUnfocused]          = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.17f, 0.21f, 0.28f, 1.00f);
	colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.62f, 0.64f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]      = accentBright;
	colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.60f, 0.22f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.70f, 0.30f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.20f, 0.23f, 1.00f);
	colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.31f, 0.32f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.24f, 0.26f, 1.00f);
	colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
	colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.56f, 0.96f, 0.35f);
	colors[ImGuiCol_DragDropTarget]        = ImVec4(0.36f, 0.65f, 1.00f, 0.90f);
	colors[ImGuiCol_NavHighlight]          = accent;
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
}

void ImGuiManager::Update()
{

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame(); // 기즈모는 매 프레임 NewFrame 직후 초기화 필요

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

	obj->AddComponent(make_shared<ModelRenderer>());
	obj->GetModelRenderer()->SetModel(model);

	CUR_SCENE->Add(obj);

	return obj->GetId();
}

int32 ImGuiManager::CreateModelAnimatorMesh(shared_ptr<Model> model, Vec3 position /*= Vec3(0,0,0)*/, int32 animIndex /*= 0*/)
{
	auto obj = make_shared<GameObject>();

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

	obj->AddComponent(make_shared<ModelAnimator>());
	obj->GetModelAnimator()->SetModel(model);
	if (animIndex >= 0)
		obj->GetModelAnimator()->GetTweenDesc().curr.animIndex = animIndex;

	CUR_SCENE->Add(obj);

	return obj->GetId();
}
