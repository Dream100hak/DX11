#pragma once


class FolderContents : public EditorWindow
{

public:
	FolderContents(Vec2 pos, Vec2 size);
	virtual ~FolderContents();

	virtual void Init() override;
	virtual void Update() override;

	void ShowFolderContents();
	void DisplayItem(const wstring& path, shared_ptr<MetaData>& meta, int32 columns, int32 id);
	void RefreshButton(ID3D11ShaderResourceView* srv, shared_ptr<MetaData>& meta, int32 id, std::function<void()> onDoubleClickCallback);

	bool IsMouseInGUIWindow(const ImVec2& scenePos, const ImVec2& sceneSize) 
	{
		const ImVec2 mousePos = ImGui::GetMousePos();
		return mousePos.x >= scenePos.x && mousePos.x <= scenePos.x + sceneSize.x &&
			mousePos.y >= scenePos.y && mousePos.y <= scenePos.y + sceneSize.y;
	}

	void CreateMeshPreviewObj(shared_ptr<MetaData>& meta);

public:
	 unordered_map<wstring, shared_ptr<GameObject>>& GetMeshPreviewObjs() { return _meshPreviewObjs; }
	 unordered_map<wstring, shared_ptr<MeshThumbnail>>& GetMeshPreviewThumbnails() { return _meshPreviewthumbnails; }

private:
	

	shared_ptr<GameObject> _meshPreviewCamera = nullptr;

	unordered_map<wstring,  shared_ptr<GameObject>> _meshPreviewObjs;
	unordered_map<wstring,  shared_ptr<MeshThumbnail>> _meshPreviewthumbnails;



};

