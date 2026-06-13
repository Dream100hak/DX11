#include "pch.h"
#include "Project.h"
#include "Utils.h"
#include "LogWindow.h"
#include "EditorToolManager.h"

#include "Camera.h"
#include "Model.h"
#include "Material.h"
#include "ModelRenderer.h"

#include "MeshThumbnail.h"
#include "ShadowMap.h"
#include "OBBBoxCollider.h"

#include "ModelMesh.h"

#include <filesystem>

Project::Project(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
	auto folder = RESOURCES->Load<Texture>(L"Folder", L"..\\Resources\\Assets\\Textures\\Folder.png");
	auto text = RESOURCES->Load<Texture>(L"Text", L"..\\Resources\\Assets\\Textures\\Text.png");

}

Project::~Project()
{
}

void Project::Init()
{
	_rootDirectory = GetDirectoryAbove(GetExecutablePath()) + L"\\Resources";
	RefreshCasheFileList(_rootDirectory);
	SELECTED_FOLDER = L"..\\Resources\\Assets\\";
}

void Project::Update()
{
	ShowProject(); // 위치/크기는 도크가 결정
}


shared_ptr<Model> Project::CreateModelFile(shared_ptr<MetaData> metaData, const wstring& modelName, const wstring& modelPath)
{
	shared_ptr<Model> model = make_shared<Model>();

	model->ReadModel(modelName + L'/' + modelName);

	// .mmat(바이너리) 우선, 없으면 레거시 .xml 폴백 (UfbxConverter 산출물은 .mmat 만 생성)
	wstring mmatPath = L"../Resources/Assets/Models/" + modelName + L'/' + modelName + L".mmat";
	if (filesystem::exists(mmatPath))
		model->ReadMaterial(modelName + L'/' + modelName);
	else
		model->ReadMaterialByXml(modelName + L'/' + modelName);

	RESOURCES->Add(metaData->fileFullPath + L'/' + modelName, model);
	return model;
}

void Project::Refresh()
{
	// 1) 사라진 파일/폴더 정리 — 삭제·이름변경·외부 제거 반영 (증분 스캔은 추가만 하므로 필요)
	for (auto it = CASHE_FILE_LIST.begin(); it != CASHE_FILE_LIST.end(); )
	{
		std::error_code ec;
		if (filesystem::exists(it->first, ec) == false)
			it = CASHE_FILE_LIST.erase(it);
		else
			++it;
	}

	// 2) 새 항목 추가 (이미 캐시된 항목은 스킵)
	RefreshCasheFileList(_rootDirectory);
}

void Project::RefreshCasheFileList(const wstring& directory)
{
	wstring searchPath = directory + L"\\*.*";
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &findFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		const std::wstring fileName = findFileData.cFileName;
		if (fileName == L"." || fileName == L"..")
			continue;

		wstring fullPath = directory + L"\\" + fileName;

		// 이미 캐시된 항목은 리소스 재로딩(Load/모델 생성) 없이 건너뛴다 — Refresh 재호출 시
		// 중복 로드/부작용 방지. 폴더는 새 하위 파일이 생겼을 수 있어 재귀는 계속한다.
		if (CASHE_FILE_LIST.find(fullPath) != CASHE_FILE_LIST.end())
		{
			if (TOOL->GetMetaType(fileName) == MetaType::FOLDER)
				RefreshCasheFileList(fullPath);
			continue;
		}

		auto meta = make_shared<MetaData>();
		meta->fileName = fileName;
		meta->fileFullPath = directory;
		meta->metaType = TOOL->GetMetaType(fileName);

		if (meta->metaType == MetaType::IMAGE)
		{
			auto tex = RESOURCES->Load<Texture>(L"TEX_" + meta->fileName, meta->fileFullPath + L"\\" + meta->fileName);
		}
		else if (meta->metaType == MetaType::MATERIAL)
		{
			// 정규화 키 — 모델(.mmat 경유)이 먼저 로드했으면 그 인스턴스를 그대로 공유 (중복 로드 금지)
			wstring matKey = Utils::ToMaterialKey(meta->fileFullPath + L'/' + meta->fileName);
			if (RESOURCES->Get<Material>(matKey) == nullptr)
			{
				shared_ptr<Material> material = make_shared<Material>();
				wstring matName = meta->fileName.substr(0, meta->fileName.find('.'));

				material->Load(meta->fileFullPath + L'/' + matName);
				RESOURCES->Add(matKey, material);
			}
		}
		else if (meta->metaType == MetaType::MESH)
		{
			wstring modelName = meta->fileName.substr(0, meta->fileName.find('.'));
			wstring modelPath = meta->fileFullPath + L'/' + modelName;
			shared_ptr<Model> model = RESOURCES->Get<Model>(modelPath);

			if (model == nullptr)
				model = CreateModelFile(meta, modelName , modelPath);
		
		}

		else if (meta->metaType == MetaType::CLIP)
		{
			auto path = filesystem::path(meta->fileFullPath);
			wstring modelName = path.filename().wstring();

			wstring modelPath = meta->fileFullPath + L'/' + modelName;
			shared_ptr<Model> model = RESOURCES->Get<Model>(modelPath);
			
			if (model == nullptr)
				model = CreateModelFile(meta, modelName, modelPath);

			wstring clipName = meta->fileName.substr(0, meta->fileName.find('.'));

			model->ReadAnimation(modelName + L'/' + clipName);

		}

		CASHE_FILE_LIST.insert({ fullPath, meta });

		if (meta->metaType == MetaType::FOLDER)
			RefreshCasheFileList(fullPath);

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}

void Project::ShowProject()
{
	ImGui::Begin("Project");
	_guiWindow = ImGui::GetCurrentWindow();

	if (ImGui::BeginTable("FolderTable", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
	{
		// 폴더 계층 구조를 표시합니다.
		ListFolderHierarchy(_rootDirectory);

		ImGui::EndTable();
	}

	ImGui::End();
}

void Project::ListFolderHierarchy(const wstring& directory, bool isForcedToggle /*= false*/)
{
	static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	string selectedPath = Utils::ToString(SELECTED_FOLDER);

	for (auto& [path, meta] : CASHE_FILE_LIST)
	{
		if (meta->metaType != FOLDER || meta->fileFullPath != directory)
			continue;

		ImGuiTreeNodeFlags node_flags = base_flags;
		string currentPath = Utils::ToString(path);
		if (selectedPath == currentPath)			
			node_flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(path.c_str());
		ImGui::AlignTextToFramePadding();

		std::string fileName = Utils::ToString(meta->fileName);
		bool node_open = ImGui::TreeNodeEx(fileName.c_str(), node_flags);

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			SELECTED_FOLDER = path;

		if (node_open)
		{
			ListFolderHierarchy(path);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
