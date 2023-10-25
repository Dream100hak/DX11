#include "pch.h"
#include "EditorTool.h"
#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "SceneCamera.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Terrain.h"
#include "Billboard.h"
#include "SnowBillboard.h"
#include "Button.h"
#include "OBBBoxCollider.h"
#include "SkyBox.h"

#include "Material.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include <boost/describe.hpp>
#include <boost/mp11.hpp>


void EditorTool::Init()
{
	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	auto shader = RESOURCES->Get<Shader>(L"Standard");

	// Camera
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Scene Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->AddComponent(make_shared<SceneCamera>());
		CUR_SCENE->Add(camera);
	}
	
	// Light
	{
		auto light = make_shared<GameObject>();
		light->SetObjectName(L"Direction Light");
		light->GetOrAddTransform()->SetPosition(Vec3(0.f));
		light->GetOrAddTransform()->SetRotation(Vec3(0.5f, 0.3f, 1.f));
		light->AddComponent(make_shared<Light>());
		LightDesc lightDesc;
		lightDesc.ambient = Vec4(1.f);
		lightDesc.diffuse = Vec4(1.f, 1.f , 1.f, 1.f);
		lightDesc.specular = Vec4(0.5f);
		lightDesc.direction = light->GetTransform()->GetRotation();
		light->GetLight()->SetLightDesc(lightDesc);
		CUR_SCENE->Add(light);
	}
	{
		
		// Object
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"SkyBox");
		obj->GetOrAddTransform();
		obj->AddComponent(make_shared<SkyBox>());
		obj->GetSkyBox()->Init();
		
		CUR_SCENE->Add(obj);
	
	}
	// Model
	{
		shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Tower/Tower");
		m2->ReadMaterial(L"Tower/Tower");

		for (int i = 0; i < 20; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Tower" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.01f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
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
			if (ImGui::MenuItem("Create Empty", "CTRL+B")) { TOOL->SetSelectedObjH(GUI->CreateEmptyGameObject()); }
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
	ImGui::SetNextWindowSize(ImVec2(1920 - 800, 30));
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

	ImGuiIO& io = ImGui::GetIO();
	if (io.NavActive == 0)
		TOOL->SetSelectedObjH(-1);

	ImGui::Begin("Hiearchy", nullptr);

	ImGui::BeginChild("left pane", ImVec2(360, 0), true);

	//	if (ImGui::IsWindowFocused() == false)
		//	TOOL->SetSelectedObjH(-1);

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


	if (SELECTED_H > -1)
	{
		shared_ptr<GameObject> go = CUR_SCENE->GetCreatedObject(SELECTED_H);

		wstring objectName = go->GetObjectName();
		string objName = string(objectName.begin(), objectName.end());

		char modifiedName[256];
		strncpy_s(modifiedName, objName.c_str(), sizeof(modifiedName));

		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.2f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

		ImGui::InputText(" ", modifiedName, sizeof(modifiedName));
		{
			go->SetObjectName(wstring(modifiedName, modifiedName + strlen(modifiedName)));
		}

		ImGui::SameLine();

		int selectedLayer = static_cast<int>(go->GetLayerIndex());
		if (ImGui::Combo("Layer", &selectedLayer, "Default\0UI\0Wall\0Invisible\0"))
		{
			go->SetLayerIndex(selectedLayer);
		}

		for (int i = 0; i < (int)ComponentType::End - 1; i++)
		{
			ComponentType componentType = static_cast<ComponentType>(i);
			shared_ptr<Component> comp = go->GetFixedComponent(componentType);

			if (comp == nullptr)
				continue;

			string s = GUI->EnumToString(componentType);

			if (ImGui::TreeNodeEx(s.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				comp->OnInspectorGUI();
	
				ImGui::TreePop();
			}
		}

		const auto& monoBehaviors = go->GetMonoBehaviours();
		for (auto& behaviors : monoBehaviors)
		{
			wstring rawName = behaviors->GetBehaviorName();
			string name = string(rawName.begin(), rawName.end());

			if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TreePop();
			}
		}


		if (ImGui::Button("Add Component", ImVec2(-1, 0)))
		{
			ImVec2 buttonPos = ImGui::GetCursorScreenPos();
			ImGui::OpenPopup("Add Component Menu");
			buttonPos.y += ImGui::GetTextLineHeightWithSpacing() * 0.4f;
			ImGui::SetNextWindowPos(buttonPos);
		}

		if (ImGui::BeginPopup("Add Component Menu"))
		{
			if (ImGui::BeginMenu("FixedComponent"))
			{
				for (int i = 0; i < (int)ComponentType::End - 1; i++)
				{
					ComponentType componentType = static_cast<ComponentType>(i);
					string fixedCompName = GUI->EnumToString(componentType);

					if (ImGui::MenuItem(fixedCompName.c_str()))
					{
						if(go->GetFixedComponent(componentType))
							continue;

						
						switch (componentType)
						{
						case ComponentType::Transform: go->AddComponent(make_shared<Transform>());
							break;
						case ComponentType::MeshRenderer: go->AddComponent(make_shared<MeshRenderer>());
							break;
					//	case ComponentType::ModelRenderer: go->AddComponent(make_shared<ModelRenderer>());
					//		break;
						case ComponentType::Camera: go->AddComponent(make_shared<Camera>());
							break;
					//	case ComponentType::Animator: go->AddComponent(make_shared<ModelAnimator>());
					//		break;
						case ComponentType::Light: go->AddComponent(make_shared<Light>());
							break;
						case ComponentType::Collider: go->AddComponent(make_shared<OBBBoxCollider>());
							break;
						case ComponentType::Terrain: go->AddComponent(make_shared<Terrain>());
							break;
						case ComponentType::Button: go->AddComponent(make_shared<Button>());
							break;
						case ComponentType::BillBoard: go->AddComponent(make_shared<Billboard>());
							break;
						//case ComponentType::SnowBillBoard: go->AddComponent(make_shared<SnowBillboard>());
						//	break;

						}
					//	go->AddComponent( GUI->CreateComponentByType(componentType) ) ;
						//shared_ptr<type_id(*this)> comp = make_shared<Component>(); // 이걸 어떻게 ComponentType에 맞게 바꾸냐고
					//	go->AddComponent("여기다가 componentType에 맞게 추가");
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("new Script"))
			{

			}

			ImGui::EndPopup();
		}

	}
	ImGui::End();
}

