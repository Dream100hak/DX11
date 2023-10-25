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

}

void Project::Update()
{
	ShowProject();
}

static std::wstring rootDirectory = L"E:\\Github\\DX11\\Project";
static std::wstring selectedDirectory = L"";  // 선택한 폴더의 경로

void Project::ShowProject()
{
	ImGui::SetNextWindowPos(ImVec2(800 + 373, 51));
	ImGui::SetNextWindowSize(ImVec2(373, 1010));
	ImGui::Begin("Project");

	if (ImGui::BeginTable("FolderTable", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
	{
		// 폴더 계층 구조를 표시합니다.
		ListFolderHierarchy(rootDirectory, 0);

		// 오른쪽 창에서 선택한 폴더 내의 파일 및 폴더를 표시합니다.
		if (!selectedDirectory.empty())
		{
			DisplayContentsOfSelectedFolder(selectedDirectory);
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void Project::ListFolderHierarchy(const std::wstring& directory, int indentation)
{
	std::wstring searchPath = directory + L"\\*.*";
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &findFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		ImGui::Text("Error listing files.");
		return;
	}

	do
	{
		if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0)
		{
			const std::wstring fileName = findFileData.cFileName;
			std::string folderName = Utils::ConvertWCharToChar(fileName.c_str());

			// 테이블의 새 행을 생성합니다.
			ImGui::TableNextRow();

			// 첫 번째 컬럼(폴더 이름)을 설정합니다.
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(folderName.c_str());
			ImGui::AlignTextToFramePadding();

			bool node_open = ImGui::TreeNodeEx(folderName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
			if (node_open)
			{

				//// 폴더가 클릭되면 selectedDirectory를 업데이트 합니다.
				if (ImGui::IsItemClicked())
				{
					selectedDirectory = directory + L"\\" + fileName;
				}

				ListFolderHierarchy(directory + L"\\" + fileName, indentation + 1);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}

void Project::DisplayContentsOfSelectedFolder(const std::wstring& directory)
{
	std::wstring searchPath = directory + L"\\*.*";
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &findFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		ImGui::Text("Error listing files.");
		return;
	}

	do
	{
		const std::wstring fileName = findFileData.cFileName;
		if (fileName == L"." || fileName == L"..")  // 이 부분을 추가
			continue;

		std::string itemName = Utils::ConvertWCharToChar(fileName.c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);  // 오른쪽 창에 표시

		if (ImGui::Selectable(itemName.c_str()))
		{
			// 이곳에서 해당 항목이 클릭되었을 때 수행할 작업을 처리하십시오.
			// 예를 들어, 파일을 열거나 다른 작업을 수행할 수 있습니다.
		}

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}
