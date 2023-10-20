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

	bool _showWindow = true;

private:
	shared_ptr<Shader> _shader;

	static bool _hiearchyWindow;
	static int32 _selectedObj;


};
