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
static std::wstring selectedDirectory = L"";  // ������ ������ ���

void Project::ShowProject()
{
	ImGui::SetNextWindowPos(ImVec2(800 + 373, 51));
	ImGui::SetNextWindowSize(ImVec2(373, 1010));
	ImGui::Begin("Project");

	if (ImGui::BeginTable("FolderTable", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable))
	{
		// ���� ���� ������ ǥ���մϴ�.
		ListFolderHierarchy(rootDirectory, 0);

		// ������ â���� ������ ���� ���� ���� �� ������ ǥ���մϴ�.
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

			// ���̺��� �� ���� �����մϴ�.
			ImGui::TableNextRow();

			// ù ��° �÷�(���� �̸�)�� �����մϴ�.
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(folderName.c_str());
			ImGui::AlignTextToFramePadding();

			bool node_open = ImGui::TreeNodeEx(folderName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
			if (node_open)
			{

				//// ������ Ŭ���Ǹ� selectedDirectory�� ������Ʈ �մϴ�.
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
		if (fileName == L"." || fileName == L"..")  // �� �κ��� �߰�
			continue;

		std::string itemName = Utils::ConvertWCharToChar(fileName.c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);  // ������ â�� ǥ��

		if (ImGui::Selectable(itemName.c_str()))
		{
			// �̰����� �ش� �׸��� Ŭ���Ǿ��� �� ������ �۾��� ó���Ͻʽÿ�.
			// ���� ���, ������ ���ų� �ٸ� �۾��� ������ �� �ֽ��ϴ�.
		}

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}
