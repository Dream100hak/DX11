#include "pch.h"
#include "Hiearchy.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"

Hiearchy::Hiearchy()
{

}

Hiearchy::~Hiearchy()
{

}

void Hiearchy::Init()
{

}

void Hiearchy::Update()
{
	ShowHiearchy();
}

void Hiearchy::ShowHiearchy()
{
	ImGui::SetNextWindowPos(ImVec2(800, 51));
	ImGui::SetNextWindowSize(ImVec2(373, 500));

	ImGuiIO& io = ImGui::GetIO();
	//if (io.NavActive == 0)
	//	TOOL->SetSelectedObjH(-1);

	ImGui::Begin("Hiearchy", nullptr);

	ImGui::BeginChild("left pane", ImVec2(360, 0), true);

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

		if (ImGui::Selectable(name.c_str(), (isSelected, ImGuiSelectableFlags_SpanAllColumns)))
		{
			CUR_SCENE->UnPickAll();

			TOOL->SetSelectedObjH(object.first);
			object.second->SetUIPicked(true);
			//TODO : 인스펙터
		}

		ImGui::PopStyleColor();
	}

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Add GameObject"))
		{
			// "Add GameObject" 클릭 시 처리할 로직
		}

		if (ImGui::MenuItem("Create Cube"))
		{
			TOOL->SetSelectedObjH(GUI->CreateMesh(CreatedObjType::CUBE));
			ADDLOG("Create Cube", LogFilter::Info);
		}
		if (ImGui::MenuItem("Create Quad"))
		{
			TOOL->SetSelectedObjH(GUI->CreateMesh(CreatedObjType::QUAD));
			ADDLOG("Create Quad", LogFilter::Info);
		}	
		if (ImGui::MenuItem("Create Sphere"))
		{
			TOOL->SetSelectedObjH(GUI->CreateMesh(CreatedObjType::SPHERE));
			ADDLOG("Create Sphere", LogFilter::Info);
		}


		ImGui::EndPopup();
	}

	ImGui::EndChild();

	ImGui::End();
}
