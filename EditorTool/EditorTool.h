#pragma once
#include "EditorWindow.h"

class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

	void OnMouseWheel(int32 scrollAmount) override;

private:

	bool _showWindow = true;

private:
	shared_ptr<class SceneCamera> _sceneCam;
};
