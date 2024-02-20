#pragma once
#include "EditorWindow.h"

class Inspector : public EditorWindow
{
public:
	Inspector();
	~Inspector();

	virtual void Init() override;
	virtual void Update() override;

	void ShowInspector();
	void ShowInfoHiearchy();
	void ShowInfoProject();

	ID3D11ShaderResourceView* GetMetaFileIcon();
};