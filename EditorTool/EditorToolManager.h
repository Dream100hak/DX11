#pragma once

class LogWindow;

class EditorToolManager
{
	DECLARE_SINGLE(EditorToolManager);

public:
	void Init();
	void Update();

	void MakeGameObject();

public:

	const unordered_map<string, shared_ptr< EditorWindow>>& GetEditorWindows() const {return _editorWindows;}
	
	const shared_ptr<EditorWindow>& GetEditorWindow(string name)  
	{
		auto it = _editorWindows.find(name);
		if (it  != _editorWindows.end())
		{
			return it->second;
		}	
	}

	void SetSelectedObjH(int64 id) { ClearId(); _selectedH = id; }
	void SetSelectedObjP(shared_ptr<MetaData> meta) { ClearId(); _selectedP = meta; }
	void ClearId() { _selectedH = -1 ; _selectedP = {};  }

	int64 GetSelectedIdH() { return _selectedH; }
	shared_ptr<MetaData>  GetSelectedIdP() { return _selectedP; }

	wstring& GetSelectedFolder() {return _selectedFolder ;}
	wstring& GetSelectedItem() {return _selectedItem; }
	map<wstring, shared_ptr<MetaData>>&  GetCashesFileList() { return _cashesFileList;}

	shared_ptr<LogWindow> GetLog();

private:
	
	 bool _hiearchyWindow = false;
	 int64 _selectedH = -1;
	 shared_ptr<MetaData> _selectedP;


private:
	unordered_map<string, shared_ptr< EditorWindow>> _editorWindows;

	wstring _selectedFolder = L"";  // 사용자가 선택한 폴더
	wstring _selectedItem = L"";  // 사용자가 선택한 폴더
	map<wstring, shared_ptr<MetaData>> _cashesFileList;

};

