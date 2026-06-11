#include "pch.h"
#include "Inspector.h"
#include "EditorToolManager.h"

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Billboard.h"

#include "Button.h"
#include "OBBBoxCollider.h"
#include "Utils.h"

// -----------------------------------------------------------
// Inspector — 하이어라키 모드 (선택된 GameObject 의 컴포넌트 표시/편집)
// -----------------------------------------------------------

void Inspector::ShowInfoHiearchy()
{
	shared_ptr<GameObject> go = CUR_SCENE->GetCreatedObject(SELECTED_H);

	wstring objectName = go->GetObjectName();
	string objName = string(objectName.begin(), objectName.end());

	char modifiedName[256];
	strncpy_s(modifiedName, objName.c_str(), sizeof(modifiedName));

	auto icon = RESOURCES->Get<Texture>(L"ObjIcon")->GetComPtr().Get();
	ImGui::Image(icon, ImVec2(40, 40));
	ImGui::SameLine();

	ImGui::BeginGroup();
	static bool active = false;
	ImGui::Checkbox("##ActiveObj", &active);

	ImVec2 layerPos = ImGui::GetItemRectMin();

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
	ImGui::InputText(" ", modifiedName, sizeof(modifiedName));
	go->SetObjectName(wstring(modifiedName, modifiedName + strlen(modifiedName)));

	ImGui::SameLine(0.f, 1.f);

	static bool staticObj = false;
	ImGui::Checkbox("Static", &staticObj);

	///////////////////////////////////////////////////
	//					 LAYER                       //
	///////////////////////////////////////////////////

	ImVec2 textPosition = ImVec2(layerPos.x, layerPos.y + 25.f);
	ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), "Layer");

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::SameLine();

	int selectedLayer = static_cast<int>(go->GetLayerIndex());
	if (ImGui::Combo("##Layer", &selectedLayer, "Default\0UI\0Wall\0Invisible\0"))
	{
		go->SetLayerIndex(selectedLayer);
	}

	ImGui::EndGroup();

	ImGui::Spacing();
	ImGui::Separator();

	///////////////////////////////////////////////////
	//               COMPONENT                      //
	///////////////////////////////////////////////////

	for (int i = 0; i < (int)ComponentType::End - 1; i++)
	{
		ComponentType componentType = static_cast<ComponentType>(i);
		shared_ptr<Component> comp = go->GetFixedComponent(componentType);

		if (comp == nullptr)
			continue;

		string name = GUI->EnumToString(componentType);
		ImGui::PushID(comp.get());
		ShowComponentInfo(comp, name);
	}

	///////////////////////////////////////////////////
	//               MONOBEHAVIOUR                   //
	///////////////////////////////////////////////////

	const auto& monoBehaviors = go->GetMonoBehaviours();
	for (auto& behaviors : monoBehaviors)
	{
		wstring rawName = behaviors->GetBehaviorName();
		string name = string(rawName.begin(), rawName.end());
		ImGui::PushID(behaviors.get());
		ShowComponentInfo(behaviors, name);
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
			std::vector<ComponentType> compTypes =
			{
				ComponentType::Camera,
				ComponentType::Light,
				ComponentType::Collider,
				ComponentType::Button,
				ComponentType::BillBoard,
			};

			std::vector<string> renderTypes =
			{
				Utils::GetClassNameEX<MeshRenderer>(),
				Utils::GetClassNameEX<ModelRenderer>(),
				Utils::GetClassNameEX<ModelAnimator>(),
			};

			for (int32 i = 0 ; i < renderTypes.size(); i++)
			{
				if (go->GetFixedComponent(ComponentType::Renderer))
					continue;

				if (ImGui::MenuItem(renderTypes[i].c_str()))
				{
					/*		switch (i)
							{
							case 0:
								go->AddComponent(make_shared<MeshRenderer>());

							case 1:
								go->AddComponent(make_shared<ModelRenderer>());

							case 2:
								go->AddComponent(make_shared<ModelAnimator>());
							}*/
				}
			}

			for (auto componentType : compTypes)
			{
				string fixedCompName = GUI->EnumToString(componentType);

				if (go->GetFixedComponent(componentType))
					continue;

				if (ImGui::MenuItem(fixedCompName.c_str()))
				{
					switch (componentType)
					{
					case ComponentType::Camera: go->AddComponent(make_shared<Camera>());
						break;
					case ComponentType::Light: go->AddComponent(make_shared<Light>());
						break;
					case ComponentType::Collider: go->AddComponent(make_shared<OBBBoxCollider>());
						break;
					case ComponentType::Button: go->AddComponent(make_shared<Button>());
						break;
					case ComponentType::BillBoard: go->AddComponent(make_shared<Billboard>());
						break;
					}
				}
			}

			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
}

void Inspector::ShowComponentInfo(shared_ptr<Component> component, string name)
{
	shared_ptr<GameObject> go = CUR_SCENE->GetCreatedObject(SELECTED_H);

	bool open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);

	float spacing = ImGui::GetStyle().ItemInnerSpacing.x + 5.f;
	ImGui::SameLine(ImGui::GetWindowWidth() - spacing - ImGui::CalcTextSize("Delete").x - ImGui::GetScrollX() - spacing);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // 빨간색
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // 마우스 오버 시 색상
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f)); // 버튼 클릭 시 색상

	if (ImGui::Button("Delete"))
	{
		ImGui::OpenPopup("Confirm Delete");
	}

	ImGui::PopStyleColor(3);

	if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to delete this component?");
		ImGui::Separator();
		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			//go->RemoveComponent(comp);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (open)
	{
		component->OnInspectorGUI();

		ImGui::TreePop();
	}

	ImGui::PopID();
	ImGui::Separator();

}
