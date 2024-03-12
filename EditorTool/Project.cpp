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
	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());
	ShowProject();
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
			shared_ptr<Material> material = make_shared<Material>();
			wstring matName = meta->fileName.substr(0, meta->fileName.find('.'));

			material->Load(meta->fileFullPath + L'/' + matName);
			RESOURCES->Add(meta->fileFullPath + L'/' + meta->fileName, material);
		}
		else if (meta->metaType == MetaType::MESH)
		{
			shared_ptr<Model> model = make_shared<Model>();
			wstring modelName = meta->fileName.substr(0, meta->fileName.find('.'));

			model->ReadModel(modelName + L'/' + modelName);
			model->ReadMaterial(modelName + L'/' + modelName);
			RESOURCES->Add(meta->fileFullPath + L'/' + meta->fileName, model);
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
