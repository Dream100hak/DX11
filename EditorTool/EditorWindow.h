#pragma once
class EditorWindow
{
public:
	EditorWindow() {}
	virtual ~EditorWindow() {}

	virtual void Init() = 0;
	virtual void Update() = 0;

};

