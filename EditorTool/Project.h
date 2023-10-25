#pragma once
#include "EditorWindow.h"

class Project : public EditorWindow
{
public:
	Project();
	~Project();

	virtual void Init() override;
	virtual void Update() override;

	void ShowProject();
	void ListFolderHierarchy(const std::wstring& directory, int indentation);
	void DisplayContentsOfSelectedFolder(const std::wstring& directory);
};