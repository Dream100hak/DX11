#pragma once
#include "EditorWindow.h"

class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

	void OnMouseWheel(int32 scrollAmount) override;

	void DrawRenderTextures(); 

private:

	bool _showWindow = true;
	shared_ptr<class SceneCamera> _sceneCam;
	shared_ptr<class Button> _btn; 

	shared_ptr<class ParticleSystem> _rainDrop;
};
