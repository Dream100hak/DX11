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

	bool IsMouseInSceneWindow(const ImVec2& scenePos, const ImVec2& sceneSize) 
	{
		const ImVec2 mousePos = ImGui::GetMousePos();
		return mousePos.x >= scenePos.x && mousePos.x <= scenePos.x + sceneSize.x &&
			mousePos.y >= scenePos.y && mousePos.y <= scenePos.y + sceneSize.y;
	}

};

