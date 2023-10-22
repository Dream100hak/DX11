#include "pch.h"
#include "EditorTool.h"
#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "CameraScript.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"

void EditorTool::Init()
{
	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	_shader = make_shared<Shader>(L"23. RenderDemo.fx");

	// Camera
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Main Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->AddComponent(make_shared<CameraScript>());
		CUR_SCENE->Add(camera);
	}
	// Light
	{
		auto light = make_shared<GameObject>();
		light->SetObjectName(L"Direction Light");
		light->AddComponent(make_shared<Light>());
		LightDesc lightDesc;
		lightDesc.ambient = Vec4(0.4f);
		lightDesc.diffuse = Vec4(1.f);
		lightDesc.specular = Vec4(0.1f);
		lightDesc.direction = Vec3(1.f, 0.f, 1.f);
		light->GetLight()->SetLightDesc(lightDesc);
		CUR_SCENE->Add(light);
	}
	{
		auto shader = make_shared<Shader>(L"18. SkyDemo.fx");

		// Material
		{
			shared_ptr<Material> material = make_shared<Material>();
			material->SetShader(shader);
			auto texture = RESOURCES->Load<Texture>(L"Sky", L"..\\Resources\\Textures\\sky01.jpg");
			material->SetDiffuseMap(texture);
			MaterialDesc& desc = material->GetMaterialDesc();
			desc.ambient = Vec4(1.f);
			desc.diffuse = Vec4(1.f);
			desc.specular = Vec4(1.f);
			RESOURCES->Add(L"Sky", material);
		}
		{
			// Object
			auto obj = make_shared<GameObject>();
			obj->SetObjectName(L"SkyBox");
			obj->GetOrAddTransform();
			obj->AddComponent(make_shared<MeshRenderer>());
			{
				auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
				obj->GetMeshRenderer()->SetMesh(mesh);
			}
			{
				auto material = RESOURCES->Get<Material>(L"Sky");
				obj->GetMeshRenderer()->SetMaterial(material);
			}

			CUR_SCENE->Add(obj);
		}
	}
	// Model
	{
		shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Tower/Tower");
		m2->ReadMaterial(L"Tower/Tower");

		for (int i = 0 ; i < 20; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Tower" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.01f));

			obj->AddComponent(make_shared<ModelRenderer>(_shader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CUR_SCENE->Add(obj);
		}
	}
}

void EditorTool::Update()
{
	GET_SINGLE(ShortcutManager)->Update();
	GET_SINGLE(EditorToolManager)->Update();

	ToolTest();
}

void EditorTool::Render()
{

}

void EditorTool::ToolTest()
{

	ImGui::ShowDemoWindow(&_showWindow);
	AppMainMenuBar();
	AppPlayMenu();
	SceneEditorWindow();
	GameEditorWindow();
	HierachyEditorWindow();
	ProjectEditorWindow();
	InspectorEditorWindow();

}

void EditorTool::AppMainMenuBar()
{
	ImVec2 menuBarSize = ImGui::GetWindowSize();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			MenuFileList();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("GameObject"))
		{
			if (ImGui::MenuItem("Create Empty", "CTRL+B")) { GUI->CreateEmptyGameObject(); }
			if (ImGui::MenuItem("Create Empty Child", "CTRL+Z")) {}
			if (ImGui::MenuItem("Create Empty Parent", "CTRL+Z")) {}
			
			ImGui::Separator();
			if (ImGui::MenuItem("2D Object", "CTRL+Z")) {}
			if (ImGui::MenuItem("3D Object", "CTRL+Z")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Component"))
		{
			if (ImGui::MenuItem("Mesh", "CTRL+Z")) {}
			if (ImGui::MenuItem("Sound", "CTRL+Z")) {}
			if (ImGui::MenuItem("Light", "CTRL+Z")) {}
			if (ImGui::MenuItem("Camera", "CTRL+Z")) {}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void EditorTool::AppPlayMenu()
{
	ImGui::SetNextWindowPos(ImVec2(800, 21));
	ImGui::SetNextWindowSize(ImVec2(1920-800, 30));
	ImGui::Begin("PlayMenu", NULL, ImGuiCol_FrameBg);

	ImGui::SetCursorPos(ImVec2((1920 - 800) * 0.5f, ImGui::GetCursorPosY()));
	if (ImGui::Button("Play", ImVec2(50, 0)))
	{

	}

	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(50, 0)))
	{
	
	}
	ImGui::End();
}

void EditorTool::MenuFileList()
{
	if (ImGui::MenuItem("New")) {}
	if (ImGui::MenuItem("Open", "Ctrl+O")) {}
	if (ImGui::MenuItem("Save", "Ctrl+S")) {}
	if (ImGui::MenuItem("Save As..")) {}

	ImGui::Separator();
	if (ImGui::BeginMenu("Options"))
	{
		static bool enabled = true;
		ImGui::MenuItem("Enabled", "", &enabled);
		ImGui::BeginChild("child", ImVec2(0, 60), true);
		for (int i = 0; i < 10; i++)
			ImGui::Text("Scrolling Text %d", i);
		ImGui::EndChild();
		static float f = 0.5f;
		static int n = 0;
		ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
		ImGui::InputFloat("Input", &f, 0.1f);
		ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Colors"))
	{
		float sz = ImGui::GetTextLineHeight();
		for (int i = 0; i < ImGuiCol_COUNT; i++)
		{
			const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
			ImGui::Dummy(ImVec2(sz, sz));
			ImGui::SameLine();
			ImGui::MenuItem(name);
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Options")) // <-- Append!
	{
		static bool b = true;
		ImGui::Checkbox("SomeOption", &b);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Disabled", false)) // Disabled
	{
		IM_ASSERT(0);
	}
	if (ImGui::MenuItem("Checked", NULL, true)) {}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4")) {}

}

void EditorTool::SceneEditorWindow()
{
	
}

void EditorTool::GameEditorWindow()
{
	GameWindowDesc gameDesc;

	ImGui::SetNextWindowPos(gameDesc.pos); // 왼쪽 상단에 위치
	ImGui::SetNextWindowSize(gameDesc.size); // 크기 설정
	ImGui::Begin("Game");
	// 게임 윈도우 내용 추가
	ImGui::End();
}

void EditorTool::HierachyEditorWindow()
{
	ImGui::SetNextWindowPos(ImVec2(800, 51)); 
	ImGui::SetNextWindowSize(ImVec2(373, 1010)); 

	ImGui::Begin("Hiearchy", nullptr);

	ImGui::BeginChild("left pane", ImVec2(360, 0), true);

	if (ImGui::IsWindowFocused() == false)
		TOOL->SetSelectedObjH(-1);
	
	const auto gameObjects = CUR_SCENE->GetCreatedObjects();
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 0.2f);

	for (auto& object : gameObjects)
	{
		wstring wstr = object.second->GetObjectName();
		if (wstr.empty())
			continue;
		string name(wstr.begin(), wstr.end());

		bool isSelected = (SELECTED_H == object.first);

		if (isSelected)
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.58f, 1.0f, 1.f)); // Blue background
		else
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 0.2f)); // Default background

		if (ImGui::Selectable(name.c_str(), (SELECTED_H == object.first, ImGuiSelectableFlags_SpanAllColumns)))
		{
			TOOL->SetSelectedObjH(object.first);
			//TODO : 인스펙터
		}

		ImGui::PopStyleColor();
	}

	ImGui::EndChild();

	ImGui::End();
}

void EditorTool::ProjectEditorWindow()
{
	ImGui::SetNextWindowPos(ImVec2(800 + 373, 51)); 
	ImGui::SetNextWindowSize(ImVec2(373, 1010)); 
	ImGui::Begin("Project");


	ImGui::End();
}

void EditorTool::InspectorEditorWindow()
{

	ImGui::SetNextWindowPos(ImVec2(800 + 373 + 373, 51)); 
	ImGui::SetNextWindowSize(ImVec2(373, 1010)); 

	ImGui::Begin("Inspector");

	ImGui::End();
}

