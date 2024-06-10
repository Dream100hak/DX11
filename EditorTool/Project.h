#pragma once

class Model;

class Project : public EditorWindow
{
public:

	Project(Vec2 pos, Vec2 size);
	virtual ~Project();

	virtual void Init() override;
	virtual void Update() override;

	void ShowProject();
	void ListFolderHierarchy(const wstring& directory , bool isForcedToggle = false);

private:
	
	shared_ptr<Model> CreateModelFile(shared_ptr<MetaData> metaData, const wstring& modelName , const wstring& modelPath );


private:

	std::wstring GetExecutablePath() {
		WCHAR path[MAX_PATH];
		GetModuleFileName(NULL, path, MAX_PATH);
		std::wstring fullPath(path);
		std::wstring::size_type pos = fullPath.find_last_of(L"\\/");
		return fullPath.substr(0, pos);
	}

	std::wstring GetDirectoryAbove(const wstring& path)
	{
		wstring::size_type pos = path.find_last_of(L"\\/");
		return path.substr(0, pos);
	}

	void RefreshCasheFileList(const wstring& directory);

private:
	wstring _rootDirectory = L"";

};