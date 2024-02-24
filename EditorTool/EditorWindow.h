#pragma once
class EditorWindow
{
public:

	EditorWindow() {}
	virtual ~EditorWindow() {}

	virtual void Init() = 0;
	virtual void Update() = 0;

	ImVec2 GetEWinPos() { return ImVec2(_winPos.x, _winPos.y); }
	ImVec2 GetEWinSize() { return ImVec2(_winSize.x , _winSize.y); }

	void SetWinPosAndSize(Vec2 pos , Vec2 size) {_winPos = pos, _winSize = size;}

protected:
	Vec2 _winPos;
	Vec2 _winSize;

};

