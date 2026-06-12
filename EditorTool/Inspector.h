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
	void DrawModelDetails(shared_ptr<class Model> model); // 요약/메시/스켈레톤 트리/머티리얼/클립
	void DrawSkeletonOverlay(shared_ptr<class Model> model, const ImVec2& imagePos, const ImVec2& imageSize); // 프리뷰 본 오버레이

	void CreateMeshPreviewObj();
	void CreateAniPreviewObj();
	void DrawInspectorMesh();
	void DrawInspectorClip();

private:

	shared_ptr<GameObject> _simpleGrid = nullptr;
	shared_ptr<GameObject> _skyBox = nullptr;

	shared_ptr<GameObject> _meshPreviewCamera = nullptr;
	shared_ptr<GameObject> _meshPreviewLight = nullptr;

	std::wstring _previewObjName = L"";
	shared_ptr<GameObject> _meshPreviewObj = nullptr;
	shared_ptr<MeshThumbnail> _meshthumbnail = nullptr;

	// 스켈레톤 — 트리 선택 본(트랜스폼 표시/오버레이 강조) + 프리뷰 오버레이 토글
	shared_ptr<struct ModelBone> _selectedBone = nullptr;
	bool _showSkeleton = true;

private:

	bool _isPlayingAnim = false; 
	float _animationProgress = 0.f;

	
};