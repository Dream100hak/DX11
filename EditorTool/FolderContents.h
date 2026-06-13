#pragma once
#include <filesystem>

class MeshThumbnail;

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

	// 우클릭 컨텍스트 — 이름 변경 / 삭제 (확인 모달 + 파일시스템 처리 후 Folder 목록 갱신)
	void DrawRenameModal();
	void DrawDeleteModal();
	void RenameItem(shared_ptr<MetaData> meta, const string& newNameUtf8);
	// 모델 묶음 리네임 — 폴더 + .mesh + .mmat(+레거시 .xml) 파일명 + .mmat 내부 머티리얼 경로까지.
	// (엔진은 Models/<이름>/<이름>.mesh 규약이라 폴더/파일명/내부경로가 모두 일치해야 로드됨)
	void RenameModelBundle(shared_ptr<MetaData> meta, const string& newBaseUtf8);
	void DeleteItem(shared_ptr<MetaData> meta);
	void RefreshProject();

	std::wstring CreateUniqueMaterialName(const std::wstring& folder, const std::wstring& baseName, const std::wstring& extension)
	{
		wstring finalName = baseName;
		int count = 0;
		std::filesystem::path finalPath = std::filesystem::path(folder) / (finalName + extension);

		while (std::filesystem::exists(finalPath)) {
			finalName = baseName + L" (" + std::to_wstring(++count) + L")";
			finalPath = std::filesystem::path(folder) / (finalName + extension);
		}

		return finalPath.wstring();
	}

	void CreateMeshPreviewObj(shared_ptr<MetaData>& meta);
	void CreateModelPreviewObj(shared_ptr<MetaData>& meta);

	void CreateAniPreviewObj(shared_ptr<MetaData>& meta);

	void CreateMeshPreviewThumbnail(shared_ptr<MetaData>& meta , shared_ptr<GameObject>& obj);



public:
	 unordered_map<wstring, shared_ptr<GameObject>>& GetMeshPreviewObjs() { return _meshPreviewObjs; }
	 unordered_map<wstring, shared_ptr<MeshThumbnail>>& GetMeshPreviewThumbnails() { return _meshPreviewthumbnails; }
	 unordered_map<wstring, float>& GetMeshScales() { return _meshScales; }

	 shared_ptr<class Camera> GetCamera() { return _meshPreviewCamera->GetCamera(); }
	 shared_ptr<class Light> GetLight() { return _meshPreviewLight->GetLight(); }

private:
	
	void DragModelFileToGUIWnd(shared_ptr<MetaData>& meta, const wstring& modelPath, shared_ptr<GameObject> obj);
	
	 

private:

	shared_ptr<GameObject> _meshPreviewCamera = nullptr;
	shared_ptr<GameObject> _meshPreviewLight = nullptr;
	unordered_map<wstring,  shared_ptr<GameObject>> _meshPreviewObjs;
	unordered_map<wstring,  shared_ptr<MeshThumbnail>> _meshPreviewthumbnails;
	deque<wstring> _thumbnailOrder; // ?몃꽕??罹먯떆 FIFO ?쒓굅 ?쒖꽌 (?곹븳 愿由?

	unordered_map<wstring, float> _meshScales;

private:

	float _displayBtnWidth = 75.f;
	float _displayBtnHeight = 75.f;

	// 컨텍스트 메뉴 대상 + 모달 트리거
	shared_ptr<MetaData> _ctxTarget = nullptr;
	bool _openRename = false;
	bool _openDelete = false;
	char _renameBuf[256] = {};

};

