#include "pch.h"
#include "FolderContents.h"

#include "Camera.h"
#include "Model.h"
#include "Material.h"
#include "ModelRenderer.h"

#include "MeshThumbnail.h"
#include "ShadowMap.h"
#include "OBBBoxCollider.h"

#include "ModelMesh.h"

#include "LogWindow.h"
#include "SceneWindow.h"
#include "Hiearchy.h"
#include "Utils.h"

FolderContents::FolderContents(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
}

FolderContents::~FolderContents()
{

}

void FolderContents::Init()
{

}

void FolderContents::Update()
{

	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());
	ShowFolderContents();
}

void FolderContents::ShowFolderContents()
{
	ImGui::Begin("Folder Contents");

	if (!SELECTED_FOLDER.empty()) {
		std::vector<std::pair<std::wstring, shared_ptr<MetaData>>> folders;
		std::vector<std::pair<std::wstring, shared_ptr<MetaData>>> files;

		for (auto& [path, meta] : CASHE_FILE_LIST)
		{
			if (meta->fileFullPath == SELECTED_FOLDER)
			{
				if (meta->metaType == MetaType::FOLDER)
				{
					folders.push_back({ path, meta });
				}
				else
				{
					files.push_back({ path, meta });
				}
			}
		}

		float windowWidth = ImGui::GetContentRegionAvail().x;
		int itemWidth = 100;
		int columns = max(1, static_cast<int>(windowWidth / itemWidth));

		if (ImGui::BeginTable("FolderTable", columns, ImGuiTableFlags_Sortable | ImGuiTableFlags_NoBordersInBody)) {

			int32 folderId = 0;
			for (auto& [path, meta] : folders)
			{
				DisplayItem(path, meta, columns, folderId++);
			}
			int32 fileId = 0;
			for (auto& [path, meta] : files)
			{
				DisplayItem(path, meta, columns, fileId);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

void FolderContents::DisplayItem(const wstring& path, shared_ptr<MetaData>& meta, int32 columns, int32 id)
{

	ImGui::TableNextColumn();

	float cellWidth = ImGui::GetColumnWidth();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // 투명 배경
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // 마우스 오버시 검정색
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // 클릭시 검정색

	float cursorX = (cellWidth - 50) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	// 폴더 처리
	if (meta->metaType == MetaType::FOLDER)
	{
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		RefreshButton(tex->GetComPtr().Get(), meta, id, [=]() { SELECTED_FOLDER = meta->fileFullPath + L"\\" + meta->fileName; ADDLOG("Move Folder Path", LogFilter::Info); });
	}

	// 이미지 파일 처리
	else if (meta->metaType == MetaType::IMAGE)
	{
		auto tex = RESOURCES->Get<Texture>(L"FILE_" + meta->fileName);
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// 문서 파일 처리
	else if (meta->metaType == MetaType::XML)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}

	// 메시 파일 처리
	if (meta->metaType == MetaType::MESH)
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->AddComponent(make_shared<Camera>());
		camera->GetOrAddTransform()->SetPosition(Vec3(0, 1.f, -4.f));
		camera->GetCamera()->UpdateMatrix();

		auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
		shared_ptr<Model> model = make_shared<Model>();
		wstring modelName = meta->fileName.substr(0, meta->fileName.find('.'));

		model->ReadModel(modelName + L'/' + modelName);
		model->ReadMaterial(modelName + L'/' + modelName);

		BoundingBox box = model->CalculateModelBoundingBox();
		float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
		float globalScale = MODEL_GLOBAL_SCALE;

		if (modelScale > 10.f)
			modelScale = globalScale;

		float scale = globalScale / modelScale;

		auto obj = make_shared<GameObject>();

		obj->GetOrAddTransform()->SetPosition(Vec3::Zero);
		obj->GetOrAddTransform()->SetRotation(Vec3(0, -0.5f, 0));
		obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

		obj->AddComponent(make_shared<ModelRenderer>(shader));
		obj->GetModelRenderer()->SetModel(model);
		obj->GetModelRenderer()->SetPass(1);

		shared_ptr<MeshThumbnail> thumbnail = GRAPHICS->GetMeshThumbnail();
		thumbnail->SetModelAndCam(obj->GetModelRenderer(), camera->GetCamera());
		thumbnail->SetWorldMatrix(obj->GetOrAddTransform()->GetWorldMatrix());
		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});

		ImVec2 scenePos = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinPos();
		ImVec2 sceneSize = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinSize();

		ImVec2 hiearchyPos = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinPos();
		ImVec2 hiearchySize = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinSize();


		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			MetaData* metaRawPtr = meta.get();

			ImGui::SetDragDropPayload("MeshPayload", &metaRawPtr,  sizeof(MetaData*));

			if (IsMouseInSceneWindow(scenePos, sceneSize))
			{
				::SetCursor(LoadCursor(NULL, IDC_HAND));
			}
			else if (IsMouseInSceneWindow(hiearchyPos, hiearchySize))
			{
				::SetCursor(LoadCursor(NULL, IDC_HAND));
			}
			else
			{
				::SetCursor(LoadCursor(NULL, IDC_NO));
			}

			ImGui::EndDragDropSource();
		}
	}
	// 예외 파일 처리
	else if (meta->metaType == MetaType::Unknown)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}

	ImGui::PopStyleColor(3);

	std::string itemName = Utils::ToString(meta->fileName);
	ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());
	cursorX = (cellWidth - textSize.x) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	ImGui::Text(itemName.c_str()); // 이름 표시
}

void FolderContents::RefreshButton(ID3D11ShaderResourceView* srv, shared_ptr<MetaData>& meta, int32 id, std::function<void()> onDoubleClickCallback)
{
	ImGui::PushID(id);
	if (srv != nullptr)
	{
		if (ImGui::ImageButton(srv, ImVec2(50, 50))) {
			SELECTED_ITEM = meta->fileFullPath;
			TOOL->SetSelectedObjP(meta);
		}

		if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (onDoubleClickCallback)
				onDoubleClickCallback();
		}
	}
	ImGui::PopID();
}
