#include "pch.h"
#include "Inspector.h"
#include "EditorToolManager.h"

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Billboard.h"

#include "Button.h"
#include "OBBBoxCollider.h"

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
	// Active/Static 토글은 아직 엔진에 연결되지 않음 — 비활성 표시
	static bool active = false;
	ImGui::BeginDisabled();
	ImGui::Checkbox("##ActiveObj", &active);
	ImGui::EndDisabled();

	ImVec2 layerPos = ImGui::GetItemRectMin();

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
	ImGui::InputText(" ", modifiedName, sizeof(modifiedName));
	go->SetObjectName(wstring(modifiedName, modifiedName + strlen(modifiedName)));

	ImGui::SameLine(0.f, 1.f);

	static bool staticObj = false;
	ImGui::BeginDisabled();
	ImGui::Checkbox("Static", &staticObj);
	ImGui::EndDisabled();

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

			// 렌더러 추가는 모델/메시 에셋 연결이 필요해 메뉴만으로는 동작 불가 — 항목 제거
			// (모델 배치는 FolderContents 드래그앤드롭 사용)

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
			// 메인 카메라의 Camera 는 렌더 루프가 의존하므로 삭제 금지
			bool isMainCamera = component->GetType() == ComponentType::Camera
				&& CUR_SCENE->GetMainCamera() == go;

			// Transform 은 RemoveComponent 내부에서 거부됨. MonoBehaviour(Script 타입)도 no-op (범위 외)
			if (isMainCamera == false)
				go->RemoveComponent(component->GetType());

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
