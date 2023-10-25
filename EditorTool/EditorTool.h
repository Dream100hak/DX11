#pragma once

class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

public:
	void ToolTest();
	// MENU // 
	void AppMainMenuBar();
	void AppPlayMenu();
	void MenuFileList();

	// EditorWindow //
	void SceneEditorWindow();
	void GameEditorWindow();
	void HierachyEditorWindow();
	void ProjectEditorWindow();
	void InspectorEditorWindow();
	void ListFolderHierarchy(const std::wstring& directory, int indentation);
	void DisplayContentsOfSelectedFolder(const std::wstring& directory);

	bool _showWindow = true;

};
