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
	if (ImGui::BeginDragDropTargetCustom(ImRect(GetEWinPos(), GetEWinPos() + GetEWinSize()), ImGui::GetID("Scene")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MeshPayload"))
		{
			MetaData** metaPtr = static_cast<MetaData**>(payload->Data);
			shared_ptr<MetaData> metaData = std::make_shared<MetaData>(**metaPtr);

			if(metaData->metaType == MetaType::MESH)
			{
				shared_ptr<Model> model = make_shared<Model>();
				wstring modelName = metaData->fileName.substr(0, metaData->fileName.find('.'));
				
				model->ReadModel(modelName + L'/' + modelName);
				model->ReadMaterial(modelName + L'/' + modelName);

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

				// 카메라와 오브젝트 사이의 기본 거리를 정의합니다.
				float distance = 10.0f; // 이 값을 조정하여 카메라가 오브젝트로부터 떨어지는 거리를 변경할 수 있습니다.

				// 카메라가 오브젝트를 바라보는 방향 벡터를 계산합니다.
				Vec3 lookDirection = cameraTransform->GetLook();
				lookDirection.Normalize(); // 방향 벡터는 항상 정규화되어야 합니다.

				// 카메라를 오브젝트의 위치로 이동시키되, 바라보는 방향의 반대 방향으로 일정 거리 떨어지도록 합니다.
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
