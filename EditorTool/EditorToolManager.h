#pragma once

class EditorWindow;
class LogWindow;

class EditorToolManager
{
	DECLARE_SINGLE(EditorToolManager);

public:
	void Init();
	void Update();

	void SetSelectedObjH(int32 id) { _selectedIdH = id; }
	void SetSelectedObjP(int32 id) { _selectedIdP = id; }

	int32 GetSelectedIdH() { return _selectedIdH; }
	int32 GetSelectedIdP() { return _selectedIdP; }
	
	shared_ptr<LogWindow> GetLog();

private:
	
	 bool _hiearchyWindow = false;
	 int32 _selectedIdH = -1;
	 int32 _selectedIdP = -1; 

private:
	unordered_map<string, shared_ptr<class EditorWindow>> _editorWindows;

};

