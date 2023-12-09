#include "pch.h"
#include "Project.h"
#include "Utils.h"

Project::Project()
{
	
}

Project::~Project()
{

}

void Project::Init()
{
	_rootDirectory = GetDirectoryAbove(GetExecutablePath()) + L"\\Project";
	RefreshCasheFileList(_rootDirectory);
}

void Project::Update()
{
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
	if(idx == string::npos)
		return MetaType::Folder;


	wstring ext = name.substr(idx + 1);
	
	if(ext == L"txt" || ext == L"TXT")
		return MetaType::Text;
	
	else if (ext == L"meta" || ext == L"META")
		return MetaType::Meta;

	else if (ext == L"wav" || ext == L"mp3" )
		return MetaType::Sound;

	else
		return MetaType::UnKnown;
}

void Project::ShowProject()
{
	//ImGui::SetNextWindowPos(ImVec2(800 + 373, 51));
	//ImGui::SetNextWindowSize(ImVec2(373, 500));

	ImGui::SetNextWindowPos(ImVec2(800, 551));
	ImGui::SetNextWindowSize(ImVec2(373 * 2, 500));
	ImGui::Begin("Project");

	if (ImGui::BeginTable("FolderTable", 1 , ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
	{
		// 폴더 계층 구조를 표시합니다.
		ListFolderHierarchy(_rootDirectory);

		ImGui::EndTable();
	}

	DisplayContentsOfSelectedFolder(_selectedDirectory);

	ImGui::End();
}

void Project::ListFolderHierarchy(const wstring& directory)
{

	for (auto& [path, meta] : _cashesFileList)
	{
		if (meta.type != Folder || meta.path != directory)
			continue;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(path.c_str());
		ImGui::AlignTextToFramePadding();

		std::string fileName = Utils::ToString(meta.name);

		if(ImGui::TreeNodeEx(fileName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
		{
			//DisplayContentsOfSelectedFolder( path );
			ListFolderHierarchy(path);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
void Project::DisplayContentsOfSelectedFolder(const std::wstring& directory)
{
	if (ImGui::BeginChild("FileList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))  // 스크롤 가능한 영역 시작
	{
		for (auto& [path, meta] : _cashesFileList)
		{
			// 현재 디렉토리와 일치하는 항목만 확인
			if (meta.path != directory)
				continue;

			std::string itemName = Utils::ToString(meta.name);

			if (ImGui::Selectable(itemName.c_str()))
			{
				// 파일 또는 폴더가 선택될 때의 동작을 여기에 추가
			}
		}
	}
	ImGui::EndChild();  // 스크롤 가능한 영역 끝
}


