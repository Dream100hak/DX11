#include "pch.h"
#include "UndoManager.h"
#include "SceneSerializer.h"
#include "EditorToolManager.h"
#include "LogWindow.h"

deque<string> UndoManager::_undoStack;
deque<string> UndoManager::_redoStack;

namespace
{
	constexpr size_t MAX_HISTORY = 32;
}

void UndoManager::Record()
{
	// 플레이 중 변형은 Stop 시 스냅샷 복원으로 롤백되므로 기록 안 함
	if (TOOL->IsPlaying())
		return;

	string snapshot;
	if (SceneSerializer::SaveToString(snapshot) == false)
		return;

	_undoStack.push_back(std::move(snapshot));
	if (_undoStack.size() > MAX_HISTORY)
		_undoStack.pop_front();

	_redoStack.clear(); // 새 액션 — redo 분기 무효
}

void UndoManager::Undo()
{
	if (_undoStack.empty())
		return;

	string current;
	SceneSerializer::SaveToString(current);
	_redoStack.push_back(std::move(current));

	SceneSerializer::LoadFromString(_undoStack.back());
	_undoStack.pop_back();

	ADDLOG("Undo", LogFilter::Info);
}

void UndoManager::Redo()
{
	if (_redoStack.empty())
		return;

	string current;
	SceneSerializer::SaveToString(current);
	_undoStack.push_back(std::move(current));

	SceneSerializer::LoadFromString(_redoStack.back());
	_redoStack.pop_back();

	ADDLOG("Redo", LogFilter::Info);
}

void UndoManager::Clear()
{
	_undoStack.clear();
	_redoStack.clear();
}

bool UndoManager::CanUndo() { return _undoStack.empty() == false; }
bool UndoManager::CanRedo() { return _redoStack.empty() == false; }
