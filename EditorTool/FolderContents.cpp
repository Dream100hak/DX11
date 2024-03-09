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

#include "Project.h"
#include "FileUtils.h"

FolderContents::FolderContents(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
}

FolderContents::~FolderContents()
{

}

void FolderContents::Init()
{
	if (_meshPreviewCamera == nullptr)
	{
		_meshPreviewCamera = make_shared<GameObject>();
		_meshPreviewCamera->AddComponent(make_shared<Camera>());
		_meshPreviewCamera->GetOrAddTransform()->SetPosition(Vec3(-1.5f, 1.f, -4.f));
		_meshPreviewCamera->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.35f, 0.f));
		_meshPreviewCamera->GetCamera()->UpdateMatrix();
	}
}

void FolderContents::Update()
{
	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());
	ShowFolderContents();
}

void FolderContents::PopupContextMenu()
{
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Create Material"))
		{
			CreateMaterial();
		}

		ImGui::EndPopup();
	}
}


void FolderContents::ShowFolderContents()
{
	ImGui::Begin("Folder Contents");

	if (!SELECTED_FOLDER.empty()) 
	{
		vector<pair<wstring, shared_ptr<MetaData>>> folders;
		vector<pair<wstring, shared_ptr<MetaData>>> files;

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

		if (ImGui::BeginTable("FolderTable", columns, ImGuiTableFlags_Sortable | ImGuiTableFlags_NoBordersInBody))
		{
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

	PopupContextMenu();

	ImGui::End();
}

void FolderContents::DisplayItem(const wstring& path, shared_ptr<MetaData>& meta, int32 columns, int32 id)
{
	ImGui::TableNextColumn();

	float cellWidth = ImGui::GetColumnWidth();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // ���� ���
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // ���콺 ������ ������
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Ŭ���� ������

	float cursorX = (cellWidth - 75) * 0.5f; // �߾� ���� ���
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	// ���� ó��
	if (meta->metaType == MetaType::FOLDER)
	{
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		RefreshButton(tex->GetComPtr().Get(), meta, id, [=]() 
		{ 
			SELECTED_FOLDER = meta->fileFullPath + L"\\" + meta->fileName;  
			ADDLOG("Move Folder Path", LogFilter::Info);
		});
	}

	// �̹��� ���� ó��
	else if (meta->metaType == MetaType::IMAGE)
	{
		auto tex = RESOURCES->Get<Texture>(L"TEX_" + meta->fileName);
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// ���� ���� ó��
	else if (meta->metaType == MetaType::XML)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// ���͸��� ���� ó��
	else if (meta->metaType == MetaType::MATERIAL)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// �޽� ���� ó��
	if (meta->metaType == MetaType::MESH)
	{
		shared_ptr<GameObject> obj = nullptr;

		/////////////// Create Mesh Preview Obj ////////////////////////
		if (_meshPreviewObjs.find(L"MODEL_" + meta->fileName) == _meshPreviewObjs.end())
		{
			CreateMeshPreviewObj(meta);
		}
			
		obj = _meshPreviewObjs[L"MODEL_" + meta->fileName];

		/////////////// Create Mesh Preview Thumbnail ////////////////////////
		shared_ptr<MeshThumbnail> thumbnail = nullptr;

		if (_meshPreviewthumbnails.find(L"MODEL_" + meta->fileName) == _meshPreviewthumbnails.end())
		{
			CreateMeshPreviewThumbnail(meta , obj);
		}
		
		thumbnail = _meshPreviewthumbnails[L"MODEL_" + meta->fileName];

		/////////////////////////////////////////////////////////////////////////

		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});

		ImVec2 scenePos = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinPos();
		ImVec2 sceneSize = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinSize();

		ImVec2 hiearchyPos = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinPos();
		ImVec2 hiearchySize = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinSize();

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			MetaData* metaRawPtr = meta.get();

			ImGui::SetDragDropPayload("MeshPayload", &metaRawPtr,  sizeof(MetaData*));

			if (IsMouseInGUIWindow(scenePos, sceneSize))
			{	
				CUR_SCENE->Add(obj);

				::SetCursor(LoadCursor(NULL, IDC_HAND));

				int32 x = INPUT->GetMousePos().x;
				int32 y = INPUT->GetMousePos().y;

				Matrix V = MAIN_CAM->GetViewMatrix();
				Matrix P = MAIN_CAM->GetProjectionMatrix();
				Viewport& vp = GRAPHICS->GetViewport();

				Vec3 n = vp.Unproject(Vec3(x, y, 0), Matrix::Identity, V, P);
				Vec3 f = vp.Unproject(Vec3(x, y, 1), Matrix::Identity, V, P);
				
				Vec3 start =  MAIN_CAM->GetTransform()->GetPosition();
				Vec3 direction = f - n;
				direction.Normalize();

				float rayLength = 1000.0f; 
				float t = -start.y / direction.y; // y=0

				if (t > 0 && t < rayLength) {
					Vec3 hitPoint = start + direction * t; // ���� ����
					obj->GetTransform()->SetPosition(hitPoint);
				}

			}
			else if (IsMouseInGUIWindow(hiearchyPos, hiearchySize))
			{
				CUR_SCENE->Remove(obj);
				::SetCursor(LoadCursor(NULL, IDC_HAND));
			}
			else
			{
				CUR_SCENE->Remove(obj);
				::SetCursor(LoadCursor(NULL, IDC_NO));
			}

			ImGui::EndDragDropSource();
		}
	}
	// ���� ���� ó��
	else if (meta->metaType == MetaType::Unknown)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}

	ImGui::PopStyleColor(3);

	string itemName = AdjustItemNameToFit(Utils::ToString(meta->fileName), _displayBtnWidth);
	ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());

	cursorX = (cellWidth - textSize.x) * 0.5f; // �߾� ���� ���
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);

	ImGui::Text(itemName.c_str()); // �̸� ǥ��
}

void FolderContents::RefreshButton(ID3D11ShaderResourceView* srv, shared_ptr<MetaData>& meta, int32 id, std::function<void()> onDoubleClickCallback)
{
	ImGui::PushID(id);
	if (srv != nullptr)
	{
		if (ImGui::ImageButton(srv, ImVec2(_displayBtnWidth, _displayBtnHeight))) {
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

void FolderContents::CreateMaterial()
{
	auto shader = RESOURCES->Get<Shader>(L"Standard");
	shared_ptr<Material> material = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();

	wstring finalPath = CreateUniqueMaterialName(SELECTED_FOLDER, L"New Material", L".mat");
	auto path = filesystem::path(finalPath);

	wstring fileName = path.filename().wstring();
	wstring directory = path.parent_path().wstring();
	filesystem::create_directory(path.parent_path());

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(finalPath, FileMode::Write);

	file->Write<int32>(5);
	file->Write<int32>(7);
	//file->Write<string>(material->name);

	auto meta = make_shared<MetaData>();
	meta->fileName = fileName;
	meta->fileFullPath = directory;
	meta->metaType = TOOL->GetMetaType(fileName);
	CASHE_FILE_LIST.insert({ finalPath, meta });

	ADDLOG("Create Material", LogFilter::Info);

}

void FolderContents::CreateMeshPreviewObj(shared_ptr<MetaData>& meta)
{
	auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
	auto model = RESOURCES->Get<Model>(L"MODEL_" + meta->fileName);
	
	BoundingBox box = model->CalculateModelBoundingBox();

	auto obj = make_shared<GameObject>();

	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = globalScale / modelScale;

	obj->GetOrAddTransform()->SetPosition(Vec3::Zero);
	obj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	obj->AddComponent(make_shared<ModelRenderer>(shader));
	obj->GetModelRenderer()->SetModel(model);
	obj->GetModelRenderer()->SetPass(1);

	_meshPreviewObjs.insert(make_pair(L"MODEL_" + meta->fileName, obj));
}

void FolderContents::CreateMeshPreviewThumbnail(shared_ptr<MetaData>& meta , shared_ptr<GameObject>& obj)
{
	shared_ptr<MeshThumbnail> thumbnail = nullptr;

	thumbnail = make_shared<MeshThumbnail>(512, 512);
	thumbnail->SetModelAndCam(obj->GetModelRenderer(), _meshPreviewCamera->GetCamera());
	thumbnail->SetWorldMatrix(obj->GetOrAddTransform()->GetWorldMatrix());

	_meshPreviewthumbnails.insert(make_pair(L"MODEL_" + meta->fileName, thumbnail));

	JOB_POST_RENDER->DoPush([=]()
	{
		thumbnail->Draw();
	});
}
