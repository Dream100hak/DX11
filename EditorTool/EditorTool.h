#pragma once
#include "EditorWindow.h"

class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

	void OnMouseWheel(int32 scrollAmount) override;

	// 외부 파일 드롭 임포트 — FBX 자동 변환 + 그 외 현재 폴더 복사 후 Folder Contents 갱신
	static void ImportDroppedFiles(const vector<wstring>& paths);

private:

	shared_ptr<class SceneCamera> _sceneCam;
	shared_ptr<class Button> _btn;

	shared_ptr<class ParticleSystem> _rainDrop;
};
