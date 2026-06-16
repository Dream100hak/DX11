#pragma once
#include "EditorWindow.h"

// ───────────────────────────────────────────────────────────
// RtDemo 에디터 패널들 (EditorTool 의 MainMenuBar/SceneWindow/Hiearchy/Inspector/
// Project/FolderContents/LogWindow 대응). 각 Update() 는 D3D12Device 의 해당
// Draw* 구현을 렌더한다(뷰=윈도우, 상태/구현=디바이스). EditorManager 가 등록·순회.
// ───────────────────────────────────────────────────────────

class MainMenuBarWindow   : public EditorWindow { public: void Update() override; };
class SceneViewWindow     : public EditorWindow { public: void Update() override; };
class HierarchyWindow      : public EditorWindow { public: void Update() override; };
class InspectorWindow      : public EditorWindow { public: void Update() override; };
class ProjectWindow        : public EditorWindow { public: void Update() override; };
class FolderContentsWindow : public EditorWindow { public: void Update() override; };
class LogPanelWindow       : public EditorWindow { public: void Update() override; };
