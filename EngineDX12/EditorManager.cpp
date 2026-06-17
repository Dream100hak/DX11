#include "EditorManager.h"
#include "EditorWindows.h"
#include "D3D12Device.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder

template<typename T>
void EditorManager::Add(const char* name)
{
	auto w = std::make_shared<T>();
	w->Bind(_dev);
	_windows.insert({ std::string(name), w });
}

void EditorManager::Init(D3D12Device* dev)
{
	_dev = dev;

	// 패널 등록 (EditorTool 의 make_shared + map insert 패턴)
	Add<MainMenuBarWindow>("MainMenuBar");
	Add<SceneViewWindow>("Scene");
	Add<HierarchyWindow>("Hierarchy");
	Add<InspectorWindow>("Inspector");
	Add<ProjectWindow>("Project");
	Add<FolderContentsWindow>("FolderContents");
	Add<LogPanelWindow>("Log");

	for (auto& w : _windows)
		if (w.second) w.second->Init();
}

void EditorManager::Update()
{
	// 1) 메인 메뉴바 (도킹 호스트보다 먼저 — 작업영역 높이 산출)
	_windows.at("MainMenuBar")->Update();

	// 2) 전체화면 도킹 호스트 (배경 없음 → 중앙 패스스루로 뒤의 3D 씬뷰 노출)
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->WorkPos);
	ImGui::SetNextWindowSize(vp->WorkSize);
	ImGui::SetNextWindowViewport(vp->ID);
	ImGuiWindowFlags host = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("DockHost", nullptr, host);
	ImGui::PopStyleVar(3);

	ImGuiID dockId = ImGui::GetID("EngineDX12Dock");

	// 최초 1회(또는 View>Reset Layout) 기본 레이아웃 — Hierarchy 좌 / Inspector 우 / Project·Log 하단 / 중앙 = 씬뷰
	static bool built = false;
	if (!built || (_dev && _dev->_resetLayout))
	{
		built = true;
		if (_dev) _dev->_resetLayout = false;
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
		ImGuiID center = dockId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
		ImGuiID bleft = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.28f, nullptr, &bottom);
		ImGui::DockBuilderDockWindow("Hierarchy", left);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("Project", bleft);
		ImGui::DockBuilderDockWindow("FolderContents", bottom);
		ImGui::DockBuilderDockWindow("Log", bottom);
		ImGui::DockBuilderDockWindow("Scene", center);
		ImGui::DockBuilderDockWindow("Game", center); // Scene 옆 탭
		ImGui::DockBuilderFinish(dockId);
	}
	ImGui::DockSpace(dockId, ImVec2(0, 0));
	ImGui::End();

	// 3) 패널 윈도우 (메뉴바 제외 — 각자 ImGui::Begin/End)
	for (auto& w : _windows)
	{
		if (!w.second || w.first == "MainMenuBar") continue;
		w.second->Update();
	}

	// 4) ImGui 드로우 데이터 빌드
	ImGui::Render();
}
