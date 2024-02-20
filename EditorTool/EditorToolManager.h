#pragma once

class EditorWindow;
class LogWindow;

class EditorToolManager
{
	DECLARE_SINGLE(EditorToolManager);

public:
	void Init();
	void Update();

	void SetSelectedObjH(int64 id) { ClearId(); _selectedH = id; }
	void SetSelectedObjP(MetaData meta) { ClearId(); _selectedP = meta; }
	void ClearId() { _selectedH = -1 ; _selectedP = {};  }

	int64 GetSelectedIdH() { return _selectedH; }
	MetaData GetSelectedIdP() { return _selectedP; }

	shared_ptr<LogWindow> GetLog();

private:
	
	 bool _hiearchyWindow = false;
	 int64 _selectedH = -1;
	 MetaData _selectedP;


private:
	unordered_map<string, shared_ptr<class EditorWindow>> _editorWindows;

};

