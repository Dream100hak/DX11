#pragma once
class FileManager
{
	DECLARE_SINGLE(FileManager);

public:
	void Init();
	void Update();
	void Render();

private:
	
	vector<GameObject> _vecForward;

};

