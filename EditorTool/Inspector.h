#pragma once
#include "EditorWindow.h"

class Inspector : public EditorWindow
{
public:
	Inspector(Vec2 pos, Vec2 size);
	virtual ~Inspector();

	virtual void Init() override;
	virtual void Update() override;

	void ShowInspector();
	void ShowInfoHiearchy();
	void ShowInfoProject();

	void PickMaterialTexture(string textureType, OUT bool& changed);

public:

	ID3D11ShaderResourceView* GetMetaFileIcon();
	shared_ptr<class MeshThumbnail>& GetMeshThumbnail();

private:

	// InspectorHierarchy.cpp — 하이어라키 선택 오브젝트 표시
	void ShowComponentInfo(shared_ptr<Component> component,  string name);

	// InspectorProject.cpp — 프로젝트 선택 파일 타입별 표시
	void ShowProjectImage(shared_ptr<MetaData>& metaData, ID3D11ShaderResourceView* icon);
	void ShowProjectMaterial(shared_ptr<MetaData>& metaData, ID3D11ShaderResourceView* icon);
	void ShowProjectMesh(shared_ptr<MetaData>& metaData);
	void ShowProjectClip(shared_ptr<MetaData>& metaData);

	void CreateMeshPreviewObj();
	void CreateAniPreviewObj();
	void DrawInspectorMesh();
	void DrawInspectorClip();

private:

	shared_ptr<GameObject> _simpleGrid = nullptr;
	shared_ptr<GameObject> _skyBox = nullptr;
	
//	shared_ptr<GameObject> _sceneGrid = nullptr;
	vector<shared_ptr<GameObject>> _sceneGrids;

	shared_ptr<GameObject> _meshPreviewCamera = nullptr;
	shared_ptr<GameObject> _meshPreviewLight = nullptr;

	std::wstring _previewObjName = L"";
	shared_ptr<GameObject> _meshPreviewObj = nullptr;
	shared_ptr<MeshThumbnail> _meshthumbnail = nullptr;

private:

	bool _isPlayingAnim = false; 
	float _animationProgress = 0.f;

	
};