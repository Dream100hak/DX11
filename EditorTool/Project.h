#pragma once
#include "EditorWindow.h"

enum MetaType
{
	None = 0,
	Folder,
	Meta, 
	Text,
	Sound,
	Image,
	Mesh,
	Xml,
	UnKnown,
};

struct MetaData
{
	wstring name;
	wstring path;
	MetaType type; 
	//TODO : 
};

class Project : public EditorWindow
{
public:

	Project();
	~Project();

	virtual void Init() override;
	virtual void Update() override;

	std::wstring GetExecutablePath() {
		WCHAR path[MAX_PATH];
		GetModuleFileName(NULL, path, MAX_PATH);
		std::wstring fullPath(path);
		std::wstring::size_type pos = fullPath.find_last_of(L"\\/");
		return fullPath.substr(0, pos);
	}

	std::wstring GetDirectoryAbove(const wstring& path) {
		std::wstring::size_type pos = path.find_last_of(L"\\/");
		return path.substr(0, pos);
	}

	MetaType GetMetaType(const wstring& name);


	void ShowProject();
	void ListFolderHierarchy(const wstring& directory);
	// ���ο� �Լ� �߰�: ���õ� ������ ������ �����ִ� â
	void ShowFolderContents();
	void DisplayItem(const std::wstring& path, const MetaData& meta, int columns);

private:

	void RefreshCasheFileList(const wstring& directory);


private:
	wstring _rootDirectory = L"";
	wstring _selectedDirectory = L"";  // ������ ������ ���
	wstring _selectedFolder = L"";  // ����ڰ� ������ ����
	wstring _selectedItem = L"";  // ����ڰ� ������ ����
	map<wstring , MetaData> _cashesFileList; 

};