#pragma once
#include <filesystem>

class FolderContents : public EditorWindow
{

public:
	FolderContents(Vec2 pos, Vec2 size);
	virtual ~FolderContents();

	virtual void Init() override;
	virtual void Update() override;

	void PopupContextMenu();
	void ShowFolderContents();
	
	void DisplayItem(const wstring& path, shared_ptr<MetaData>& meta, int32 columns, int32 id);
	void RefreshButton(ID3D11ShaderResourceView* srv, shared_ptr<MetaData>& meta, int32 id, std::function<void()> onDoubleClickCallback);

	std::string AdjustItemNameToFit(const std::string& itemName, float buttonWidth) {
		
		ImVec2 textSize = ImGui::CalcTextSize(itemName.c_str());

		if (textSize.x > buttonWidth) {
			std::string adjustedName = itemName;
			float ellipsisTextSize = ImGui::CalcTextSize("...").x;

			while (!adjustedName.empty() && (ImGui::CalcTextSize(adjustedName.c_str()).x + ellipsisTextSize > buttonWidth)) {
				adjustedName.pop_back();
			}

			if (!adjustedName.empty()) {
				adjustedName += "...";
			}

			return adjustedName;
		}

		return itemName;
	}


	bool IsMouseInGUIWindow(const ImVec2& scenePos, const ImVec2& sceneSize) 
	{
		const ImVec2 mousePos = ImGui::GetMousePos();
		return mousePos.x >= scenePos.x && mousePos.x <= scenePos.x + sceneSize.x &&
			mousePos.y >= scenePos.y && mousePos.y <= scenePos.y + sceneSize.y;
	}

	void CreateMaterial();
	std::wstring CreateUniqueMaterialName(const std::wstring& folder, const std::wstring& baseName, const std::wstring& extension)
	{
		wstring finalName = baseName;
		int count = 0;
		std::filesystem::path finalPath = std::filesystem::path(folder) / (finalName + extension);

		// 파일이 이미 존재하는지 확인
		while (std::filesystem::exists(finalPath)) {
			finalName = baseName + L" (" + std::to_wstring(++count) + L")";
			finalPath = std::filesystem::path(folder) / (finalName + extension);
		}

		return finalPath.wstring();
	}

	void CreateMeshPreviewObj(shared_ptr<MetaData>& meta);
	void CreateModelPreviewObj(shared_ptr<MetaData>& meta);
	void CreateMeshPreviewThumbnail(shared_ptr<MetaData>& meta , shared_ptr<GameObject>& obj);

public:
	 unordered_map<wstring, shared_ptr<GameObject>>& GetMeshPreviewObjs() { return _meshPreviewObjs; }
	 unordered_map<wstring, shared_ptr<MeshThumbnail>>& GetMeshPreviewThumbnails() { return _meshPreviewthumbnails; }

	 shared_ptr<class Camera> GetCamera() { return _meshPreviewCamera->GetCamera(); }
	 shared_ptr<class Light> GetLight() { return _meshPreviewLight->GetLight(); }

private:

	shared_ptr<GameObject> _meshPreviewCamera = nullptr;
	shared_ptr<GameObject> _meshPreviewLight = nullptr;
	unordered_map<wstring,  shared_ptr<GameObject>> _meshPreviewObjs;
	unordered_map<wstring,  shared_ptr<MeshThumbnail>> _meshPreviewthumbnails;

private:

	float _displayBtnWidth = 75.f;
	float _displayBtnHeight = 75.f;

};

