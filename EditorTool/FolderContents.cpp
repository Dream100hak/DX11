#include "pch.h"
#include "FolderContents.h"

#include "Camera.h"
#include "Model.h"
#include "Material.h"
#include "MeshRenderer.h"
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

#include "InstancingBuffer.h"
#include "terrain.h"

#include "Light.h"

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
		_meshPreviewCamera->SetObjectName(L"Preview Cam");
		_meshPreviewCamera->AddComponent(make_shared<Camera>());
		_meshPreviewCamera->GetOrAddTransform()->SetPosition(Vec3(-1.5f, 1.f, -4.f));
		_meshPreviewCamera->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.35f, 0.f));
		_meshPreviewCamera->GetCamera()->UpdateMatrix();
	}

	if(_meshPreviewLight == nullptr)
	{
		_meshPreviewLight = make_shared<GameObject>();
		_meshPreviewLight->GetOrAddTransform()->SetRotation(Vec3(-0.57735f, -0.57735f, 0.57735f));
		_meshPreviewLight->AddComponent(make_shared<Light>());
		LightDesc lightDesc;

		lightDesc.ambient = Vec4(1.f, 1.0f, 1.0f, 1.0f);
		lightDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightDesc.specular = Vec4(0.8f, 0.8f, 0.7f, 1.0f);
		lightDesc.direction = _meshPreviewLight->GetTransform()->GetRotation();
		_meshPreviewLight->GetLight()->SetLightDesc(lightDesc);
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

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // 투명 배경
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // 마우스 오버시 검정색
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // 클릭시 검정색

	float cursorX = (cellWidth - 75) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	// 폴더 처리
	if (meta->metaType == MetaType::FOLDER)
	{
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		RefreshButton(tex->GetComPtr().Get(), meta, id, [=]() 
		{ 
			SELECTED_FOLDER = meta->fileFullPath + L"\\" + meta->fileName;  
			ADDLOG("Move Folder Path", LogFilter::Info);
		});
	}

	// 이미지 파일 처리
	else if (meta->metaType == MetaType::IMAGE)
	{
		auto tex = RESOURCES->Get<Texture>(L"TEX_" + meta->fileName);
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// 문서 파일 처리
	else if (meta->metaType == MetaType::XML)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// 매터리얼 파일 처리
	else if (meta->metaType == MetaType::MATERIAL)
	{
		shared_ptr<GameObject> obj = nullptr;
		/////////////// Create Material Preview Obj ////////////////////////
		if (_meshPreviewObjs.find(meta->fileFullPath + L'/' + meta->fileName) == _meshPreviewObjs.end())
		{
			CreateMeshPreviewObj(meta);
		}
		obj = _meshPreviewObjs[meta->fileFullPath + L'/' + meta->fileName];
	
		/////////////// Create Mesh Preview Thumbnail ////////////////////////
		shared_ptr<MeshThumbnail> thumbnail = nullptr;

		if (_meshPreviewthumbnails.find(meta->fileFullPath + L'/' + meta->fileName) == _meshPreviewthumbnails.end())
		{
			CreateMeshPreviewThumbnail(meta, obj);
		}

		thumbnail = _meshPreviewthumbnails[meta->fileFullPath + L'/' + meta->fileName];

		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});
	}
	// 메시 파일 처리
	if (meta->metaType == MetaType::MESH)
	{
		shared_ptr<GameObject> obj = nullptr;

		/////////////// Create Mesh Preview Obj ////////////////////////
		if (_meshPreviewObjs.find(meta->fileFullPath + L'/' + meta->fileName) == _meshPreviewObjs.end())
		{
			CreateModelPreviewObj(meta);
		}
			
		obj = _meshPreviewObjs[meta->fileFullPath + L'/' + meta->fileName];

		/////////////// Create Mesh Preview Thumbnail ////////////////////////
		shared_ptr<MeshThumbnail> thumbnail = nullptr;

		if (_meshPreviewthumbnails.find(meta->fileFullPath + L'/' + meta->fileName) == _meshPreviewthumbnails.end())
		{
			CreateMeshPreviewThumbnail(meta , obj);
		}
		
		thumbnail = _meshPreviewthumbnails[meta->fileFullPath + L'/' + meta->fileName];

		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});

		ImVec2 scenePos = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinPos();
		ImVec2 sceneSize = TOOL->GetEditorWindow(Utils::GetClassNameEX<SceneWindow>())->GetEWinSize();

		ImVec2 hiearchyPos = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinPos();
		ImVec2 hiearchySize = TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>())->GetEWinSize();

		Vec3 prevScale = obj->GetTransform()->GetLocalScale();

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

				if (t > 0 && t < rayLength) 
				{				
					Vec3 hitPoint = start + direction * t;
					
					shared_ptr<GameObject> terrain = CUR_SCENE->GetTerrain();
					
					if(terrain)
						hitPoint.y = terrain->GetTerrain()->GetHeight(hitPoint.x, hitPoint.z);	

					obj->GetTransform()->SetScale(Vec3(6,6,6));
					obj->GetTransform()->SetPosition(hitPoint);
				}

			}
			else if (IsMouseInGUIWindow(hiearchyPos, hiearchySize))
			{
				CUR_SCENE->Remove(obj);
				obj->GetTransform()->SetScale(prevScale);
				::SetCursor(LoadCursor(NULL, IDC_HAND));
			}
			else
			{
				CUR_SCENE->Remove(obj);
				obj->GetTransform()->SetScale(prevScale);
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

	string itemName = AdjustItemNameToFit(Utils::ToString(meta->fileName), _displayBtnWidth);
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
	shared_ptr<Material> material = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();

	wstring finalPath = CreateUniqueMaterialName(SELECTED_FOLDER, L"New Material", L".mat");
	auto path = filesystem::path(finalPath);

	wstring fileName = path.filename().wstring();
	wstring directory = path.parent_path().wstring();
	filesystem::create_directory(path.parent_path());

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(finalPath, FileMode::Write);

	file->Write<string>(Utils::ToString(material->GetShader()->GetFile()));
	file->Write<Color>(material->GetMaterialDesc().ambient);	
	file->Write<Color>(material->GetMaterialDesc().diffuse);	
	file->Write<Color>(material->GetMaterialDesc().specular);	
	file->Write<Color>(material->GetMaterialDesc().emissive);

	wstring fullPath = directory + L"\\" + fileName;

	auto meta = make_shared<MetaData>();
	meta->fileName = fileName;
	meta->fileFullPath = directory;
	meta->metaType = TOOL->GetMetaType(fileName);
	CASHE_FILE_LIST.insert({ fullPath, meta });
	RESOURCES->Add(meta->fileFullPath + L'/' + meta->fileName, material);

	string logStr = Utils::ToString(L"Create Material : " + finalPath);
	ADDLOG(logStr, LogFilter::Info);
}

void FolderContents::CreateMeshPreviewObj(shared_ptr<MetaData>& meta)
{
	shared_ptr<Mesh> mesh = make_shared<Mesh>();
	auto mat = RESOURCES->Get<Material>(meta->fileFullPath + L'/' + meta->fileName);
	wstring debugPath = meta->fileFullPath + L'/' + meta->fileName;

	mesh->CreateSphere();
	auto obj = make_shared<GameObject>();
	obj->AddComponent(make_shared<MeshRenderer>());
	obj->SetObjectName(L"MAT_" + meta->fileName);
	obj->GetOrAddTransform()->SetPosition(Vec3(1.929f, 1.2f, 5.394f));
	obj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	obj->GetOrAddTransform()->SetScale(Vec3(7.f, 7.f, 7.f));
	obj->GetMeshRenderer()->SetMesh(mesh);
	obj->GetMeshRenderer()->SetMaterial(mat);

	if(mat->GetDiffuseMap() == nullptr && mat->GetNormalMap() == nullptr && mat->GetSpecularMap() == nullptr)
		obj->GetMeshRenderer()->SetTechnique(2);

	_meshPreviewObjs.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, obj));
}

void FolderContents::CreateModelPreviewObj(shared_ptr<MetaData>& meta)
{
	auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
	auto model = RESOURCES->Get<Model>(meta->fileFullPath + L'/' + meta->fileName);
	
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

	_meshPreviewObjs.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, obj));

}

void FolderContents::CreateMeshPreviewThumbnail(shared_ptr<MetaData>& meta , shared_ptr<GameObject>& obj)
{
	shared_ptr<MeshThumbnail> thumbnail = nullptr;
	thumbnail = make_shared<MeshThumbnail>(1024, 1024);

	InstancingData data;
	data.world = obj->GetTransform()->GetWorldMatrix();
	data.isPicked = obj->GetUIPicked() ? 1 : 0;
	shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
	buffer->AddData(data);

	switch (meta->metaType)
	{
		case MATERIAL:

			JOB_POST_RENDER->DoPush([=]()
			{		
				thumbnail->Draw(obj->GetMeshRenderer(), _meshPreviewCamera->GetCamera(), _meshPreviewLight->GetLight() , buffer);
			});
			break;
		case MESH:

			JOB_POST_RENDER->DoPush([=]()
			{
				thumbnail->Draw(obj->GetModelRenderer(), _meshPreviewCamera->GetCamera(), _meshPreviewLight->GetLight(),  buffer);
			});
			break;
	}

	_meshPreviewthumbnails.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, thumbnail));

}

