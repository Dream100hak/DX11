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
	//_camera = make_shared<GameObject>();
	//_camera->AddComponent(make_shared<Camera>());
	//_camera->GetOrAddTransform()->SetPosition(Vec3(0, 1.f, -4.f));
	//_camera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, true);
	//CUR_SCENE->Add(_camera);


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
		meta.name = fileName;
		meta.path = directory;
		meta.type = GetMetaType(fileName);

		_cashesFileList.insert({ fullPath, meta });

		if (meta.type == MetaType::Folder)
			RefreshCasheFileList(fullPath);

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}

MetaType Project::GetMetaType(const wstring& name)
{
	size_t idx = name.find('.');
	if (idx == string::npos)
		return MetaType::Folder;

	wstring ext = name.substr(idx + 1);

	if (ext == L"txt" || ext == L"TXT")
		return MetaType::Text;

	else if (ext == L"meta" || ext == L"META")
		return MetaType::Meta;

	else if (ext == L"wav" || ext == L"mp3")
		return MetaType::Sound;

	else if (ext == L"jpg" || ext == L"png" || ext == L"dds")
		return MetaType::Image;

	else if (ext == L"mesh")
		return MetaType::Mesh;

	else if (ext == L"xml" || ext == L"XML")
		return MetaType::Xml;

	else
		return MetaType::UnKnown;
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
		if (meta.type != Folder || meta.path != directory)
			continue;

		ImGuiTreeNodeFlags node_flags = base_flags;
		string currentPath = Utils::ToString(path);
		if (selectedPath == currentPath)
			node_flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(path.c_str());
		ImGui::AlignTextToFramePadding();

		std::string fileName = Utils::ToString(meta.name);
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

		for (auto& [path, meta] : _cashesFileList) {
			if (meta.path == _selectedFolder) {
				if (meta.type == MetaType::Folder) {
					folders.push_back({ path, meta });
				}
				else {
					files.push_back({ path, meta });
				}
			}
		}

		float windowWidth = ImGui::GetContentRegionAvail().x;
		int itemWidth = 100;
		int columns = max(1, static_cast<int>(windowWidth / itemWidth));

		if (ImGui::BeginTable("FolderTable", columns, ImGuiTableFlags_Sortable | ImGuiTableFlags_NoBordersInBody)) {
			for (auto& [path, meta] : folders) {
				DisplayItem(path, meta, columns);
			}
			for (auto& [path, meta] : files) {
				DisplayItem(path, meta, columns);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

void Project::DisplayItem(const std::wstring& path, const MetaData& meta, int columns) 
{
	
	ImGui::TableNextColumn();

	float cellWidth = ImGui::GetColumnWidth();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // 투명 배경
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // 마우스 오버시 검정색
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); // 클릭시 검정색

	float cursorX = (cellWidth - 50) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);

	// 폴더 처리
	if (meta.type == MetaType::Folder) {
		auto tex = RESOURCES->Get<Texture>(L"Folder");
		if (tex != nullptr) {
			if (ImGui::ImageButton(tex->GetComPtr().Get(), ImVec2(50, 50))) {
				_selectedItem = path;
			}
		}
	}

	// 이미지 파일 처리
	else if (meta.type == MetaType::Image) {
		auto tex = RESOURCES->Load<Texture>(L"FILE_" + meta.name, meta.path + L"\\" + meta.name);
		if (tex != nullptr)
		{
			if (ImGui::ImageButton(tex->GetComPtr().Get(), ImVec2(50, 50))) {
				_selectedItem = path;
			}
		}

	}
	// 문서 파일 처리
	else if (meta.type == MetaType::Xml) {
		auto tex = RESOURCES->Get<Texture>(L"Text");
		if (tex != nullptr)
		{
			if (ImGui::ImageButton(tex->GetComPtr().Get(), ImVec2(50, 50))) {
				_selectedItem = path;
			}
		}
	}

	// 메시 파일 처리
	if (meta.type == MetaType::Mesh) 
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->AddComponent(make_shared<Camera>());
		camera->GetOrAddTransform()->SetPosition(Vec3(0, 1.f, -4.f));
		camera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, true);
		camera->GetCamera()->UpdateMatrix();

		auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
		shared_ptr<Model> model = make_shared<Model>();
		wstring modelName = meta.name.substr(0, meta.name.find('.'));

		model->ReadModel(modelName + L'/' + modelName);
		model->ReadMaterial(modelName + L'/' + modelName);

		BoundingBox box = model->CalculateModelBoundingBox();
		float modelDiagonal = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
		
		if(modelDiagonal > 10.f)
			modelDiagonal = _modelDiagonal;

		float scale = _modelDiagonal / modelDiagonal;

		auto obj = make_shared<GameObject>();

		obj->GetOrAddTransform()->SetPosition(Vec3::Zero);
		obj->GetOrAddTransform()->SetRotation(Vec3(0, -0.65f, 0));
		obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));
	
		obj->AddComponent(make_shared<ModelRenderer>(shader));
		obj->GetModelRenderer()->SetModel(model);
		obj->GetModelRenderer()->SetPass(1);

		shared_ptr<MeshThumbnail> thumbnail = GRAPHICS->GetMeshThumbnail();
		thumbnail->SetModelAndCam(obj->GetModelRenderer(), camera->GetCamera());
		thumbnail->SetWorldMatrix(obj->GetOrAddTransform()->GetWorldMatrix());
		if (ImGui::ImageButton((void*)thumbnail->GetComPtr().Get(), ImVec2(50, 50))) {
			_selectedItem = path;
		}
	}
	// 예외 파일 처리
	else if (meta.type == MetaType::UnKnown) {
		auto tex = RESOURCES->Get<Texture>(L"Text");
		if (tex != nullptr)
		{
			if (ImGui::ImageButton(tex->GetComPtr().Get(), ImVec2(50, 50))) {
				_selectedItem = path;
			}
		}
	}

	ImGui::PopStyleColor(3);

	std::string itemName = Utils::ToString(meta.name);
	ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());
	cursorX = (cellWidth - textSize.x) * 0.5f; // 중앙 정렬 계산
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
	ImGui::Text(itemName.c_str()); // 이름 표시
}
