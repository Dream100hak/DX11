#pragma once
#include "EditorWindow.h"

class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	vector<shared_ptr<EditorWindow>> _editorWindows; 
	bool _showWindow = true;
};
