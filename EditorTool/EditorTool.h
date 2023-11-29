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
	shared_ptr<class SceneCamera> _sceneCam;
	shared_ptr<class Button> _btn; 

	Matrix _view;
	Matrix _projection;
	Matrix _mvp;
	Matrix _vp;

	Vec3 _translationPlan;
	Vec3 _rayOrigin;
	Vec3 _rayDir;

	shared_ptr<Transform> _tr; 

};
