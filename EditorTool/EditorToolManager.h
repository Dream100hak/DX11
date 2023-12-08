#pragma once

class EditorWindow;
class LogWindow;

class EditorToolManager
{
	DECLARE_SINGLE(EditorToolManager);

public:
	void Init();
	void Update();

	void SetSelectedObjH(int64 id) { _selectedIdH = id; }
	void SetSelectedObjP(int64 id) { _selectedIdP = id; }

	int64 GetSelectedIdH() { return _selectedIdH; }
	int64 GetSelectedIdP() { return _selectedIdP; }
	
	shared_ptr<LogWindow> GetLog();

private:
	
	 bool _hiearchyWindow = false;
	 int64 _selectedIdH = -1;
	 int64 _selectedIdP = -1;


private:
	unordered_map<string, shared_ptr<class EditorWindow>> _editorWindows;

};

