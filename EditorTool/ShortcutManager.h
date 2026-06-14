#pragma once


class ShortcutManager
{
	DECLARE_SINGLE(ShortcutManager);

public:
	void Init();
	void Update();

	void File();
	void Menu();
	void CreateEmpty();
	void DeleteObject();

private:
	int64 _clipboardId = -1; // Ctrl+C 로 복사한 오브젝트 id (Ctrl+V 로 반복 붙여넣기)
};

