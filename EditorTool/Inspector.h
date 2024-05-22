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

	void ShowComponentInfo(shared_ptr<Component> component,  string name);

};