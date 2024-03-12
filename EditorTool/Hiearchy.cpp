#include "pch.h"
#include "Hiearchy.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"


#include "Model.h"

Hiearchy::Hiearchy(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
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
	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());

	ImGuiIO& io = ImGui::GetIO();
	//if (io.NavActive == 0)
	//	TOOL->SetSelectedObjH(-1);

	ImGui::Begin("Hiearchy", nullptr);

	// 드롭 가능 영역 설정
	if (ImGui::BeginDragDropTargetCustom(ImRect(GetEWinPos(), GetEWinPos() + GetEWinSize()), ImGui::GetID("Hiearchy")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MeshPayload"))
		{
			MetaData** metaPtr = static_cast<MetaData**>(payload->Data);
			shared_ptr<MetaData> metaData = std::make_shared<MetaData>(**metaPtr);

			if(metaData->metaType == MetaType::MESH)
			{
				auto model = RESOURCES->Get<Model>(metaData->fileFullPath + L"/" + metaData->fileName);

				int32 id = GUI->CreateModelMesh(model);
				CUR_SCENE->UnPickAll();
				TOOL->SetSelectedObjH(id);
				CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);
				
				ADDLOG("Create Model", LogFilter::Info);
			}

			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}
		ImGui::EndDragDropTarget();
	}


	ImGui::BeginChild("left pane", ImVec2(360, 0), true);

	const auto gameObjects = CUR_SCENE->GetCreatedObjects();
	ImGuiStyle& style = ImGui::GetStyle();

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
		}

		if (isSelected && ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			auto camera = SCENE->GetCurrentScene()->GetMainCamera();
			if (camera)
			{
				Vec3 objPos = object.second->GetTransform()->GetPosition();
				auto cameraTransform = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();

				float distance = 10.0f; 

				Vec3 lookDirection = cameraTransform->GetLook();
				lookDirection.Normalize(); 

				Vec3 cameraPosition = objPos - (lookDirection * distance);

				cameraTransform->SetPosition(cameraPosition);
			}
		}

		ImGui::PopStyleColor();
	}

	if (ImGui::BeginPopupContextWindow())
	{
		int32 id = -1;

		if (ImGui::MenuItem("Add GameObject"))
		{	
			id = GUI->CreateEmptyGameObject(); 
			TOOL->SetSelectedObjH(id);
			ADDLOG("Create GameObject", LogFilter::Info);
		}

		if (ImGui::MenuItem("Create Cube"))
		{
			id = GUI->CreateMesh(CreatedObjType::CUBE);
			TOOL->SetSelectedObjH(id);
			ADDLOG("Create Cube", LogFilter::Info);
		}
		if (ImGui::MenuItem("Create Quad"))
		{
			id = GUI->CreateMesh(CreatedObjType::QUAD);
			TOOL->SetSelectedObjH(id);
			ADDLOG("Create Quad", LogFilter::Info);
		}	
		if (ImGui::MenuItem("Create Sphere"))
		{
			id = GUI->CreateMesh(CreatedObjType::SPHERE);
			TOOL->SetSelectedObjH(id);
			ADDLOG("Create Sphere", LogFilter::Info);
		}

		if (id != -1)
		{
			CUR_SCENE->UnPickAll();
			CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();

	ImGui::End();
}
