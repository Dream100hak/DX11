#include "pch.h"
#include "Hiearchy.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"

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
			//TODO : ¿ŒΩ∫∆Â≈Õ
		}

		ImGui::PopStyleColor();
	}

	ImGui::EndChild();

	ImGui::End();
}
