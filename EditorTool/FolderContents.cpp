#include "pch.h"
#include "FolderContents.h"

#include "Camera.h"
#include "Model.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"

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
	// 드래그 중이 아니면 떠도는 프리뷰(에디터-내부 모델)를 씬에서 제거.
	// 프리뷰는 드래그 중에만 씬에 올라가고 드롭 시 SceneWindow 가 내리지만,
	// 드롭이 오버레이/창에 먹히거나 취소되면 씬에 남아 "붙어서 같이 온" 팬텀처럼 보였다.
	if (ImGui::GetDragDropPayload() == nullptr)
	{
		for (auto& [k, pv] : _meshPreviewObjs)
			if (pv) CUR_SCENE->Remove(pv);
	}

	ShowFolderContents(); // 위치/크기는 도크가 결정
}

void FolderContents::PopupContextMenu()
{
	// 빈 공간에서만 — 아이템 위 우클릭은 항목별 Rename/Delete 메뉴(RefreshButton)가 잡도록
	// (NoOpenOverItems 없으면 이 창 메뉴가 아이템 위에서도 열려 항목 메뉴를 덮어버림)
	if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
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

	// 우클릭 메뉴에서 요청한 이름변경/삭제 모달 (테이블 루프 밖에서 한 번)
	DrawRenameModal();
	DrawDeleteModal();

	ImGui::End();
}

void FolderContents::DrawRenameModal()
{
	if (_openRename)
	{
		_openRename = false;
		if (_ctxTarget != nullptr)
		{
			string name = Utils::ToString(_ctxTarget->fileName);
			strncpy_s(_renameBuf, sizeof(_renameBuf), name.c_str(), _TRUNCATE);
		}
		ImGui::OpenPopup("Rename##fc");
	}

	if (ImGui::BeginPopupModal("Rename##fc", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("New name (확장자 포함):");
		ImGui::SetNextItemWidth(320.f);
		const bool enter = ImGui::InputText("##renamebuf", _renameBuf, sizeof(_renameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::TextDisabled("주의: 연결 파일(.mesh<->.mmat 등)은 자동으로 같이 바뀌지 않습니다.");

		if (ImGui::Button("OK", ImVec2(80, 0)) || enter)
		{
			RenameItem(_ctxTarget, _renameBuf);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void FolderContents::DrawDeleteModal()
{
	if (_openDelete)
	{
		_openDelete = false;
		ImGui::OpenPopup("Delete##fc");
	}

	if (ImGui::BeginPopupModal("Delete##fc", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (_ctxTarget != nullptr)
		{
			const bool isFolder = (_ctxTarget->metaType == MetaType::FOLDER);
			ImGui::Text("'%s' %s 삭제할까요?",
				Utils::ToString(_ctxTarget->fileName).c_str(),
				isFolder ? "폴더(하위 포함)를" : "파일을");
		}
		ImGui::TextDisabled("되돌릴 수 없습니다.");

		if (ImGui::Button("Delete", ImVec2(80, 0)))
		{
			DeleteItem(_ctxTarget);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void FolderContents::RenameItem(shared_ptr<MetaData> meta, const string& newNameUtf8)
{
	if (meta == nullptr)
		return;

	// 모델(.mesh/.mmat)은 폴더+파일+내부경로를 함께 바꾸는 묶음 리네임으로 위임
	if (meta->metaType == MetaType::MESH || meta->metaType == MetaType::MODEL_MAT)
	{
		RenameModelBundle(meta, newNameUtf8);
		return;
	}

	wstring newName = Utils::ToWString(newNameUtf8);
	if (newName.empty() || newName == meta->fileName)
		return;

	filesystem::path dir(meta->fileFullPath);
	filesystem::path oldPath = dir / meta->fileName;
	filesystem::path newPath = dir / newName;

	std::error_code ec;
	if (filesystem::exists(newPath, ec))
	{
		ADDLOG("Rename: 같은 이름이 이미 있습니다 - " + newNameUtf8, LogFilter::Warn);
		return;
	}

	const bool wasSelected = (SELECTED_P == meta);

	filesystem::rename(oldPath, newPath, ec);
	if (ec)
	{
		ADDLOG("Rename 실패: " + ec.message(), LogFilter::Warn);
		return;
	}

	// 이전 경로 키 캐시 제거 (새 항목은 Refresh 가 추가)
	const wstring oldKey = meta->fileFullPath + L'/' + meta->fileName;
	_meshPreviewthumbnails.erase(oldKey);
	_meshPreviewObjs.erase(oldKey);
	_meshScales.erase(oldKey);

	// 폴더 이름 변경 시 현재 열린 경로가 그 아래면 새 경로로 보정
	if (meta->metaType == MetaType::FOLDER)
	{
		const wstring oldFull = oldPath.wstring();
		const wstring newFull = newPath.wstring();
		if (SELECTED_FOLDER == oldFull || SELECTED_FOLDER.rfind(oldFull + L"\\", 0) == 0)
			SELECTED_FOLDER = newFull + SELECTED_FOLDER.substr(oldFull.size());
	}

	ADDLOG("Rename: " + Utils::ToString(meta->fileName) + " -> " + newNameUtf8, LogFilter::Info);
	RefreshProject();

	// 이전 meta 는 사라진 파일을 가리킴 — 선택 중이었으면 새 이름 항목으로 옮긴다
	// (안 옮기면 인스펙터가 없는 파일 텍스처를 로드하려다 크래시)
	if (wasSelected)
	{
		shared_ptr<MetaData> newMeta = nullptr;
		for (auto& [p, m] : CASHE_FILE_LIST)
			if (m->fileFullPath == meta->fileFullPath && m->fileName == newName) { newMeta = m; break; }
		TOOL->SetSelectedObjP(newMeta); // 못 찾으면 nullptr (선택 해제)
	}
}

void FolderContents::RenameModelBundle(shared_ptr<MetaData> meta, const string& newBaseUtf8)
{
	// 새 베이스명 — 사용자가 확장자를 붙여도 stem 만 사용
	wstring newBase = filesystem::path(Utils::ToWString(newBaseUtf8)).stem().wstring();
	if (newBase.empty())
		return;

	filesystem::path folder(meta->fileFullPath);   // .../Models/Tower
	const wstring oldBase = folder.filename().wstring(); // Tower (규약상 mesh/mmat stem 과 동일)
	if (newBase == oldBase)
		return;

	filesystem::path parent = folder.parent_path();
	filesystem::path newFolder = parent / newBase;

	std::error_code ec;
	if (filesystem::exists(newFolder, ec))
	{
		ADDLOG("Rename: 같은 이름의 폴더가 이미 있습니다 - " + newBaseUtf8, LogFilter::Warn);
		return;
	}

	const bool wasSelected = (SELECTED_P == meta);
	const wstring oldPrefix = folder.wstring();

	// 1) 폴더 통째로 이동 (mesh/mmat/clip/텍스처 전부 함께)
	filesystem::rename(folder, newFolder, ec);
	if (ec)
	{
		ADDLOG("Rename 실패(folder): " + ec.message(), LogFilter::Warn);
		return;
	}

	// 2) .mmat 내부 머티리얼 경로의 폴더 부분을 새 폴더로 갱신 + 파일명 변경
	//    (.mmat = int32 count + count×string[머티리얼경로]. 머티리얼 파일명은 유지, 폴더만 교체)
	filesystem::path oldMmat = newFolder / (oldBase + L".mmat");
	if (filesystem::exists(oldMmat, ec))
	{
		vector<string> mats;
		{
			shared_ptr<FileUtils> in = make_shared<FileUtils>();
			in->Open(oldMmat.wstring(), FileMode::Read);
			int32 count = in->Read<int32>();
			for (int32 i = 0; i < count; ++i)
				mats.push_back(in->Read<string>());
		}
		for (string& s : mats)
		{
			string baseName = filesystem::path(s).filename().string(); // 예: watchtower
			s = "../Resources/Assets/Models/" + Utils::ToString(newBase) + "/" + baseName;
		}

		filesystem::path newMmat = newFolder / (newBase + L".mmat");
		{
			shared_ptr<FileUtils> out = make_shared<FileUtils>();
			out->Open(newMmat.wstring(), FileMode::Write);
			out->Write<int32>(static_cast<int32>(mats.size()));
			for (string& s : mats)
				out->Write<string>(s);
		}
		if (newMmat != oldMmat)
			filesystem::remove(oldMmat, ec);
	}

	// 3) .mesh / 레거시 .xml 파일명도 새 베이스명으로 (텍스처/클립은 이름 유지)
	auto renameInside = [&](const wstring& ext)
	{
		filesystem::path o = newFolder / (oldBase + ext);
		filesystem::path n = newFolder / (newBase + ext);
		if (o != n && filesystem::exists(o, ec))
			filesystem::rename(o, n, ec);
	};
	renameInside(L".mesh");
	renameInside(L".xml");

	// 4) 옛 폴더 경로로 캐싱된 썸네일/프리뷰/스케일 제거 (새 항목은 Refresh 가 재생성)
	auto eraseByPrefix = [&](auto& map)
	{
		for (auto it = map.begin(); it != map.end(); )
		{
			if (it->first.rfind(oldPrefix, 0) == 0)
				it = map.erase(it);
			else
				++it;
		}
	};
	eraseByPrefix(_meshPreviewthumbnails);
	eraseByPrefix(_meshPreviewObjs);
	eraseByPrefix(_meshScales);

	// 5) 열린 폴더가 이동 대상이면 새 경로로 보정
	if (SELECTED_FOLDER == oldPrefix || SELECTED_FOLDER.rfind(oldPrefix + L"\\", 0) == 0)
		SELECTED_FOLDER = newFolder.wstring() + SELECTED_FOLDER.substr(oldPrefix.size());

	ADDLOG("Rename model: " + Utils::ToString(oldBase) + " -> " + Utils::ToString(newBase), LogFilter::Info);
	RefreshProject();

	// 6) 선택을 새 .mesh 항목으로 이동
	if (wasSelected)
	{
		const wstring newMeshName = newBase + L".mesh";
		shared_ptr<MetaData> newMeta = nullptr;
		for (auto& [p, m] : CASHE_FILE_LIST)
			if (m->fileFullPath == newFolder.wstring() && m->fileName == newMeshName) { newMeta = m; break; }
		TOOL->SetSelectedObjP(newMeta);
	}
}

void FolderContents::DeleteItem(shared_ptr<MetaData> meta)
{
	if (meta == nullptr)
		return;

	filesystem::path target = filesystem::path(meta->fileFullPath) / meta->fileName;

	std::error_code ec;
	filesystem::remove_all(target, ec); // 파일/폴더(하위 포함) 모두
	if (ec)
	{
		ADDLOG("Delete 실패: " + ec.message(), LogFilter::Warn);
		return;
	}

	const wstring key = meta->fileFullPath + L'/' + meta->fileName;
	_meshPreviewthumbnails.erase(key);
	_meshPreviewObjs.erase(key);
	_meshScales.erase(key);

	// 삭제 항목을 인스펙터가 계속 가리키지 않게 선택 해제
	if (SELECTED_P == meta)
		TOOL->SetSelectedObjP(nullptr);

	// 현재 열린 폴더(또는 그 조상)를 지웠으면 부모로 이동
	if (meta->metaType == MetaType::FOLDER)
	{
		const wstring deleted = target.wstring();
		if (SELECTED_FOLDER == deleted || SELECTED_FOLDER.rfind(deleted + L"\\", 0) == 0)
			SELECTED_FOLDER = meta->fileFullPath;
	}

	ADDLOG("Delete: " + Utils::ToString(meta->fileName), LogFilter::Info);
	RefreshProject();
}

void FolderContents::RefreshProject()
{
	auto project = static_pointer_cast<Project>(TOOL->GetEditorWindow(Utils::GetClassNameEX<Project>()));
	if (project != nullptr)
		project->Refresh();
}

void FolderContents::DisplayItem(const wstring& path, shared_ptr<MetaData>& meta, int32 columns, int32 id)
{
	ImGui::TableNextColumn();

	float cellWidth = ImGui::GetColumnWidth();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // 기본 버튼 색
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // 호버 상태 색
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // 클릭 상태 버튼 색

	float cursorX = (cellWidth - 75) * 0.5f; // 중앙 정렬 X 위치
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	// 썸네일 처리
	if (meta->metaType == MetaType::FOLDER)
	{
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		RefreshButton(tex->GetComPtr().Get(), meta, id, [=]() 
		{ 
			SELECTED_FOLDER = meta->fileFullPath + L"\\" + meta->fileName;  
			ADDLOG("Move Folder Path", LogFilter::Info);
		});
	}

	// 썸네일 처리
	else if (meta->metaType == MetaType::IMAGE)
	{
		auto tex = RESOURCES->Get<Texture>(L"TEX_" + meta->fileName);
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// 썸네일 처리
	else if (meta->metaType == MetaType::XML)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}
	// 썸네일 처리
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
	// 썸네일 처리
	if (meta->metaType == MetaType::MESH)
	{
		shared_ptr<GameObject> obj = nullptr;
	
		wstring modelPath = meta->fileFullPath + L'/' + meta->fileName;

		/////////////// Create Mesh Preview Obj ////////////////////////
		if (_meshPreviewObjs.find(modelPath) == _meshPreviewObjs.end())
		{
			CreateModelPreviewObj(meta);
		}
			
		obj = _meshPreviewObjs[modelPath];

		/////////////// Create Mesh Preview Thumbnail ////////////////////////
		shared_ptr<MeshThumbnail> thumbnail = nullptr;

		if (_meshPreviewthumbnails.find(modelPath) == _meshPreviewthumbnails.end())
		{
			CreateMeshPreviewThumbnail(meta , obj);
		}
		
		thumbnail = _meshPreviewthumbnails[modelPath];

		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});

		DragModelFileToGUIWnd(meta, modelPath , obj);
	}
	// 애니메이션 처리
	else if (meta->metaType == MetaType::CLIP)
	{
		shared_ptr<GameObject> obj = nullptr;

		wstring clipPath = meta->fileFullPath + L'/' + meta->fileName;

		/////////////// Create Mesh Preview Obj ////////////////////////
		if (_meshPreviewObjs.find(clipPath) == _meshPreviewObjs.end())
		{
			CreateAniPreviewObj(meta);
		}

		obj = _meshPreviewObjs[clipPath];

		/////////////// Create Mesh Preview Thumbnail ////////////////////////
		shared_ptr<MeshThumbnail> thumbnail = nullptr;

		if (_meshPreviewthumbnails.find(clipPath) == _meshPreviewthumbnails.end())
		{
			CreateMeshPreviewThumbnail(meta, obj);
		}

		thumbnail = _meshPreviewthumbnails[clipPath];

		RefreshButton(thumbnail->GetComPtr().Get(), meta, id, []() {});

		// ???쒕옒洹몃뱶濡??뚯뒪 ??SceneWindow ??CLIP ?쒕∼ 遺꾧린(CreateModelAnimatorMesh)? ?곌껐
		DragModelFileToGUIWnd(meta, clipPath, obj);
	}
	else if (meta->metaType == MetaType::MODEL_MAT)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}

	// 썸네일 처리
	else if (meta->metaType == MetaType::Unknown)
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex->GetComPtr().Get(), meta, id, []() {});
	}

	ImGui::PopStyleColor(3);

	string itemName = AdjustItemNameToFit(Utils::ToString(meta->fileName), _displayBtnWidth);
	ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());

	cursorX = (cellWidth - textSize.x) * 0.5f; // 중앙 정렬 X 위치
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);

	ImGui::Text(itemName.c_str()); // 아이템명 표시
}

void FolderContents::RefreshButton(ID3D11ShaderResourceView* srv, shared_ptr<MetaData>& meta, int32 id, std::function<void()> onDoubleClickCallback)
{
	// meta 포인터로 고유 ID — 기존 id 는 파일끼리 전부 0 이라 충돌(컨텍스트 메뉴/상태 오작동)
	ImGui::PushID(meta.get());
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

		// 우클릭 컨텍스트 메뉴 — 이름 변경 / 삭제 (모달은 ShowFolderContents 에서 처리)
		if (ImGui::BeginPopupContextItem())
		{
			TOOL->SetSelectedObjP(meta); // 우클릭한 항목을 선택 상태로
			if (ImGui::MenuItem("Rename")) { _ctxTarget = meta; _openRename = true; }
			if (ImGui::MenuItem("Delete")) { _ctxTarget = meta; _openDelete = true; }
			ImGui::EndPopup();
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

	// 기존 수동 직렬화는 Material::Load 와 포맷 불일치(여분 wstring/count 필드)로 깨진 .mat 을 만들었음 — Save 로 교체
	material->SetName(finalPath);
	material->Save(finalPath);

	wstring fullPath = directory + L"\\" + fileName;

	auto meta = make_shared<MetaData>();
	meta->fileName = fileName;
	meta->fileFullPath = directory;
	meta->metaType = TOOL->GetMetaType(fileName);
	CASHE_FILE_LIST.insert({ fullPath, meta });
	RESOURCES->Add(Utils::ToMaterialKey(meta->fileFullPath + L'/' + meta->fileName), material);

	string logStr = Utils::ToString(L"Create Material : " + finalPath);
	ADDLOG(logStr, LogFilter::Info);
}

void FolderContents::CreateMeshPreviewObj(shared_ptr<MetaData>& meta)
{
	shared_ptr<Mesh> mesh = make_shared<Mesh>();

	// 정규화 키 — 씬 모델과 같은 .mat 인스턴스를 프리뷰/인스펙터가 공유 (편집 즉시 반영)
	auto mat = RESOURCES->Get<Material>(Utils::ToMaterialKey(meta->fileFullPath + L'/' + meta->fileName));
	if (mat == nullptr)
		mat = RESOURCES->Get<Material>(L"DefaultMaterial");

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

	obj->SetEditorInternal(true); // 프리뷰 — 드래그 중 씬에 들어와도 직렬화 제외
	_meshPreviewObjs.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, obj));
}

void FolderContents::CreateModelPreviewObj(shared_ptr<MetaData>& meta)
{
	wstring modelName = meta->fileName.substr(0, meta->fileName.find('.'));
	wstring modelPath = meta->fileFullPath + L'/' + modelName;

	auto model = RESOURCES->Get<Model>(modelPath);
	
	BoundingBox box = model->CalculateModelBoundingBox();
	auto obj = make_shared<GameObject>();

	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = globalScale / modelScale;

	//scale = 0.01f;

	obj->GetOrAddTransform()->SetPosition(Vec3::Zero);
	obj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	obj->AddComponent(make_shared<ModelRenderer>());
	obj->GetModelRenderer()->SetModel(model);

	obj->SetEditorInternal(true); // 프리뷰 — 드래그 중 씬에 들어와도 직렬화 제외
	_meshPreviewObjs.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, obj));
	_meshScales.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, scale));

}

void FolderContents::CreateAniPreviewObj(shared_ptr<MetaData>& meta)
{
	auto path = filesystem::path(meta->fileFullPath);
	wstring modelName = path.filename().wstring();
	wstring modelPath = meta->fileFullPath + L'/' + modelName;

	auto model = RESOURCES->Get<Model>(modelPath);

	BoundingBox box = model->CalculateModelBoundingBox();
	auto obj = make_shared<GameObject>();

	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	// TODO: 추가 기능
	//modelScale = .5f;

	float scale = globalScale / modelScale;
	//scale = 0.01f;

	obj->GetOrAddTransform()->SetPosition(Vec3::Zero);
	obj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	obj->AddComponent(make_shared<ModelAnimator>());
	obj->GetModelAnimator()->SetModel(model);

	obj->SetEditorInternal(true); // 프리뷰 — 드래그 중 씬에 들어와도 직렬화 제외
	_meshPreviewObjs.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, obj));
	_meshScales.insert(make_pair(meta->fileFullPath + L'/' + meta->fileName, scale));

	shared_ptr<ModelAnimator> animator = obj->GetModelAnimator();
	TweenDesc& desc = animator->GetTweenDesc();
	shared_ptr<ModelAnimation> animation = animator->GetModel()->GetAnimationByFileName(meta->fileName);

	desc.curr.animIndex = animator->GetModel()->GetAnimIndexByFileName(meta->fileName);
}

void FolderContents::CreateMeshPreviewThumbnail(shared_ptr<MetaData>& meta , shared_ptr<GameObject>& obj)
{
	std::vector<shared_ptr<Renderer>> renderers;
	std::vector<shared_ptr<InstancingBuffer>> buffers;

	renderers.push_back(obj->GetRenderer());

	shared_ptr<MeshThumbnail> thumbnail = nullptr;
	// 512: 최대 표시 크기(인스펙터 373px)보다 크면 충분 — 1024 는 개당 8MB x 캐시 64 = 최대 512MB 였음
	thumbnail = make_shared<MeshThumbnail>(512, 512);

	InstancingData data;
	data.world = obj->GetTransform()->GetWorldMatrix();
	data.isPicked = obj->GetUIPicked() ? 1 : 0;

	shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
	buffer->AddData(data);
	buffers.push_back(buffer);

	// AABB 자동 핏 — 모델 크기/위치와 무관하게 중앙 정렬 (정사각 RT 이므로 aspect 1)
	Matrix V, P;
	MeshThumbnail::ComputeFitViewProj(obj, 1.f, V, P);

	// 즉시 렌더 — 구 잡큐(JOB_POST_RENDER)는 ImGui 가 그린 뒤 실행되어 첫 프레임이 검었음
	// 폴더컨텐츠 썸네일엔 그리드 끔 (그리드는 인스펙터 프리뷰 전용)
	thumbnail->Draw(renderers, V , P , _meshPreviewLight->GetLight(), buffers, false);

	const wstring key = meta->fileFullPath + L'/' + meta->fileName;
	_meshPreviewthumbnails.insert(make_pair(key, thumbnail));
	_thumbnailOrder.push_back(key);

	// ?몃꽕??罹먯떆 ?곹븳 ??1024x1024 RT 媛 ?먯궛 ?섎쭔??臾댄븳 利앹떇?섎뜕 ?꾩닔 諛⑹?.
	// 媛???ㅻ옒 ?꾩뿉 留뚮뱺 寃껊????쒓굅 (FIFO). ?붾㈃??蹂댁씠硫??ㅼ쓬 ?꾨젅?꾩뿉 lazy ?ъ깮??
	// ?좏깮 以묒씤 ??ぉ/諛⑷툑 留뚮뱺 ??ぉ? 蹂댄샇 (Inspector 媛 operator[] 濡?吏곸젒 李몄“).
	constexpr size_t MAX_THUMBNAIL_CACHE = 64;
	wstring selectedKey;
	if (auto selected = SELECTED_P)
		selectedKey = selected->fileFullPath + L'/' + selected->fileName;

	size_t guard = _thumbnailOrder.size();
	while (_thumbnailOrder.size() > MAX_THUMBNAIL_CACHE && guard-- > 0)
	{
		wstring oldest = _thumbnailOrder.front();
		_thumbnailOrder.pop_front();

		if (oldest == selectedKey || oldest == key)
		{
			_thumbnailOrder.push_back(oldest); // 蹂댄샇 ??ぉ? ?ㅻ줈 ?뚯쟾
			continue;
		}
		_meshPreviewthumbnails.erase(oldest);
	}
}

void FolderContents::DragModelFileToGUIWnd(shared_ptr<MetaData>& meta, const wstring& modelPath, shared_ptr<GameObject> obj)
{
	// 도킹 도입 후 창 위치는 유동 — 고정 좌표 대신 라이브 rect 사용
	const SceneDesc& sceneDesc = GAME->GetSceneDesc();
	ImVec2 scenePos(sceneDesc.x, sceneDesc.y);
	ImVec2 sceneSize(sceneDesc.width, sceneDesc.height);

	ImVec2 hiearchyPos(0.f, 0.f);
	ImVec2 hiearchySize(0.f, 0.f);
	if (ImGuiWindow* hierWnd = ImGui::FindWindowByName("Hiearchy"))
	{
		hiearchyPos = hierWnd->Pos;
		hiearchySize = hierWnd->Size;
	}

	float prevScale = _meshScales[modelPath];
	float scaleRatio = prevScale * 6;

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		MetaData* metaRawPtr = meta.get();

		ImGui::SetDragDropPayload("MeshPayload", &metaRawPtr, sizeof(MetaData*));

		if (IsMouseInGUIWindow(scenePos, sceneSize))
		{
			CUR_SCENE->Add(obj);

			::SetCursor(LoadCursor(NULL, IDC_HAND));

			int32 x = INPUT->GetMousePos().x;
			int32 y = INPUT->GetMousePos().y;

			// 커서 아래 지형 표면에 배치 — 카메라 레이 ↔ 지형 교차(높이필드).
			// (예전엔 y=0 평면 교차 후 GetHeight 보정 → 기복/언덕 지형에서 (x,z)가 커서와 어긋나
			//  여러 모델이 한 곳에 뭉쳐 떨어지던 버그. 브러시와 동일한 RaycastTerrain 으로 교체)
			Vec3 o, d;
			CUR_SCENE->ScreenToWorldRay(x, y, o, d);

			Vec3 hitPoint;
			bool hit = false;

			if (shared_ptr<GameObject> terrainObj = CUR_SCENE->GetTerrain())
			{
				if (auto terrain = terrainObj->GetTerrain())
				{
					Vec3 hp;
					if (terrain->RaycastTerrain(o, d, hp)) { hitPoint = hp; hit = true; }
				}
			}

			if (hit == false) // 지형 없음/미적중 — y=0 평면 폴백
			{
				float t = (fabsf(d.y) > 1e-5f) ? (-o.y / d.y) : -1.f;
				if (t > 0.f && t < 1000.f) { hitPoint = o + d * t; hit = true; }
			}

			if (hit)
			{
				obj->GetTransform()->SetScale(Vec3(scaleRatio));
				obj->GetTransform()->SetPosition(hitPoint);
			}

		}
		else if (IsMouseInGUIWindow(hiearchyPos, hiearchySize))
		{
			CUR_SCENE->Remove(obj);
			obj->GetTransform()->SetScale(Vec3(prevScale));
			::SetCursor(LoadCursor(NULL, IDC_HAND));
		}
		else
		{
			CUR_SCENE->Remove(obj);
			obj->GetTransform()->SetScale(Vec3(prevScale));
			::SetCursor(LoadCursor(NULL, IDC_NO));
		}

		ImGui::EndDragDropSource();
	}

}

