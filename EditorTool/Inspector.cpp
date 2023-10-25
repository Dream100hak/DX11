#include "pch.h"
#include "Inspector.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"

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

Inspector::Inspector()
{

}

Inspector::~Inspector()
{

}

void Inspector::Init()
{

}

void Inspector::Update()
{
	ShowInspector();
}

void Inspector::ShowInspector()
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
						if (go->GetFixedComponent(componentType))
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
