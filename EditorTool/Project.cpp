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


Project::Project()
{
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
}

void Project::Update()
{
	ImGui::SetNextWindowPos(ImVec2(800, 551));
	ImGui::SetNextWindowSize(ImVec2(373, 500));
	ShowProject();

	ImGui::SetNextWindowPos(ImVec2(800 + 373, 551));
	ImGui::SetNextWindowSize(ImVec2(373, 500));
	ShowFolderContents();
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

		MetaData meta = {};
		meta.fileName = fileName;
		meta.fileFullPath = directory;
		meta.metaType = GetMetaType(fileName);

		_cashesFileList.insert({ fullPath, meta });

		if (meta.metaType == MetaType::FOLDER)
			RefreshCasheFileList(fullPath);

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}

MetaType Project::GetMetaType(const wstring& name)
{
	size_t idx = name.find('.');
	if (idx == string::npos)
		return MetaType::FOLDER;

	wstring ext = name.substr(idx + 1);

	if (ext == L"txt" || ext == L"TXT")
		return MetaType::TEXT;

	else if (ext == L"meta" || ext == L"META")
		return MetaType::META;

	else if (ext == L"wav" || ext == L"mp3")
		return MetaType::SOUND;

	else if (ext == L"jpg" || ext == L"png" || ext == L"dds")
		return MetaType::IMAGE;

	else if (ext == L"mesh")
		return MetaType::MESH;

	else if (ext == L"xml" || ext == L"XML")
		return MetaType::XML;

	else
		return MetaType::Unknown;
}

void Project::ShowProject()
{

	ImGui::Begin("Project");

	if (ImGui::BeginTable("FolderTable", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
	{
		// 폴더 계층 구조를 표시합니다.
		ListFolderHierarchy(_rootDirectory);

		ImGui::EndTable();
	}

	ImGui::End();
}

void Project::ListFolderHierarchy(const wstring& directory)
{
	static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	string selectedPath = Utils::ToString(_selectedFolder); 

	for (auto& [path, meta] : _cashesFileList)
	{
		if (meta.metaType != FOLDER || meta.fileFullPath != directory)
			continue;

		ImGuiTreeNodeFlags node_flags = base_flags;
		string currentPath = Utils::ToString(path);
		if (selectedPath == currentPath)
			node_flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(path.c_str());
		ImGui::AlignTextToFramePadding();

		std::string fileName = Utils::ToString(meta.fileName);
		bool node_open = ImGui::TreeNodeEx(fileName.c_str(), node_flags);

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			_selectedFolder = path;

		if (node_open)
		{
			ListFolderHierarchy(path);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
void Project::ShowFolderContents()
{
	ImGui::Begin("Folder Contents");

	if (!_selectedFolder.empty()) {
		std::vector<std::pair<std::wstring, MetaData>> folders;
		std::vector<std::pair<std::wstring, MetaData>> files;

		for (auto& [path, meta] : _cashesFileList) 
		{
			if (meta.fileFullPath == _selectedFolder)
			{
				if (meta.metaType == MetaType::FOLDER)
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
				DisplayItem(path, meta, columns , folderId++);
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

void Project::DisplayItem(const std::wstring& path, const MetaData& meta, int32 columns, int32 id)
{
	
	ImGui::TableNextColumn();

	float cellWidth = ImGui::GetColumnWidth();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // 투명 배경
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // 마우스 오버시 검정색
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // 클릭시 검정색

	float cursorX = (cellWidth - 50) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	// 폴더 처리
	if (meta.metaType == MetaType::FOLDER) 
	{
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		RefreshButton(tex, meta, id);
	}

	// 이미지 파일 처리
	else if (meta.metaType == MetaType::IMAGE) 
	{
		auto tex = RESOURCES->Load<Texture>(L"FILE_" + meta.fileName, meta.fileFullPath + L"\\" + meta.fileName);
		RefreshButton(tex, meta, id);
	}
	// 문서 파일 처리
	else if (meta.metaType == MetaType::XML) 
	{
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex, meta, id);
	}

	// 메시 파일 처리
	if (meta.metaType == MetaType::MESH)
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->AddComponent(make_shared<Camera>());
		camera->GetOrAddTransform()->SetPosition(Vec3(0, 1.f, -4.f));
		camera->GetCamera()->UpdateMatrix();

		auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
		shared_ptr<Model> model = make_shared<Model>();
		wstring modelName = meta.fileName.substr(0, meta.fileName.find('.'));

		model->ReadModel(modelName + L'/' + modelName);
		model->ReadMaterial(modelName + L'/' + modelName);

		BoundingBox box = model->CalculateModelBoundingBox();
		float modelDiagonal = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;

		if(modelDiagonal > 10.f)
			modelDiagonal = _modelDiagonal;

		float scale = _modelDiagonal / modelDiagonal;

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
		
		ImGui::PushID(id);
		if (ImGui::ImageButton((void*)thumbnail->GetComPtr().Get(), ImVec2(50, 50)))
		{
			_selectedItem = path;
			TOOL->SetSelectedObjP(meta);
		}
		ImGui::PopID();
	}
	// 예외 파일 처리
	else if (meta.metaType == MetaType::Unknown) {
		auto tex = RESOURCES->Get<Texture>(L"Text");
		RefreshButton(tex, meta, id);
	}

	ImGui::PopStyleColor(3);

	std::string itemName = Utils::ToString(meta.fileName);
	ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());
	cursorX = (cellWidth - textSize.x) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	ImGui::Text(itemName.c_str()); // 이름 표시
}

void Project::RefreshButton(shared_ptr<Texture> texture, const MetaData& meta, int32 id)
{
	ImGui::PushID(id);
	if (texture != nullptr)
	{
		if (ImGui::ImageButton(texture->GetComPtr().Get(), ImVec2(50, 50))) {
			_selectedItem = meta.fileFullPath;
			TOOL->SetSelectedObjP(meta);
		}
	}
	ImGui::PopID();
}
