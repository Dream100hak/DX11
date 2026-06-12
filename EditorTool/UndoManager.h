#pragma once

// 씬 스냅샷 기반 Undo/Redo
// - 변형 액션 "직전"에 Record() 호출 (오브젝트 생성/삭제/페어런팅/기즈모 드래그 시작/컴포넌트 추가·삭제)
// - Ctrl+Z = Undo, Ctrl+Y = Redo (ShortcutManager), Edit 메뉴에도 배선
// - 스냅샷 = SceneSerializer XML 문자열 (씬 규모가 작아 수 KB — 32개 상한)
class UndoManager
{
public:
	static void Record();
	static void Undo();
	static void Redo();
	static void Clear(); // 씬 교체(New/Open) 시 히스토리 무효화

	static bool CanUndo();
	static bool CanRedo();

private:
	static deque<string> _undoStack;
	static deque<string> _redoStack;
};
