#include "D3D12Device.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder
#include "imgui_impl_win32.h"
#include <filesystem>

namespace fs = std::filesystem;

// wstring → UTF-8 (ImGui 텍스트용)
static std::string WToUtf8(const std::wstring& w)
{
	if (w.empty()) return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

void D3D12Device::InitEditor()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = nullptr; // imgui.ini 미저장 (런타임 아티팩트 방지)
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(_hwnd);
	_imgui.Init(_device.Get(), _queue.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, FRAME_COUNT);

	// 에셋 루트 = exe\..\Resources\Assets
	wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
	_assetRoot = fs::weakly_canonical(fs::path(dir) / L".." / L"Resources" / L"Assets").wstring();
	_curDir = _assetRoot;
	_editorReady = true;
}

// ImGui::NewFrame ~ ImGui::Render — 도킹 + 패널 구성 (Render() 초반 CPU 단계에서 호출)
void D3D12Device::BuildUI()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 전체화면 도킹 호스트 (배경 없음 → 중앙 패스스루로 뒤의 3D 씬뷰 노출)
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

	ImGuiID dockId = ImGui::GetID("RtDemoDock");

	// 최초 1회 기본 레이아웃 — Inspector 우측 / FolderContents 하단 / 중앙 = 씬뷰(패스스루)
	static bool built = false;
	if (!built)
	{
		built = true;
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
		ImGuiID center = dockId;
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("FolderContents", bottom);
		ImGui::DockBuilderFinish(dockId);
	}
	ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	DrawInspector();
	DrawFolderContents();

	ImGui::Render();
}

void D3D12Device::DrawInspector()
{
	ImGui::Begin("Inspector");

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Pos   %.2f  %.2f  %.2f", _camPos.x, _camPos.y, _camPos.z);
		ImGui::Text("Yaw %.2f   Pitch %.2f", _camYaw, _camPitch);
		ImGui::TextDisabled("RMB drag = look,  WASD = move");
		ImGui::TextDisabled("Q/E = up/down,  Shift = fast");
	}

	if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Intensity", &_lightIntensity, 0.0f, 4.0f);
		ImGui::Checkbox("Animate Sun", &_lightAnimate);
		if (!_lightAnimate)
			ImGui::SliderFloat("Sun Angle", &_lightAngle, -3.14159f, 3.14159f);
	}

	if (ImGui::CollapsingHeader("Global Illumination (DDGI)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("GI Strength", &_giStrength, 0.0f, 1.5f);
		ImGui::SliderFloat("Ambient", &_ambient, 0.0f, 0.2f);
		ImGui::TextDisabled("Probes %u  (%dx%dx%d)", PROBE_COUNT, PROBE_X, PROBE_Y, PROBE_Z);
	}

	if (ImGui::CollapsingHeader("Model: Archer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Vertices : %u", _vertexCount);
		ImGui::Text("Triangles: %u", _indexCount / 3);
		ImGui::Text("Bones    : %u", (unsigned)_bonesData.size());
		ImGui::Text("Submeshes: %u   Materials: %u", (unsigned)_submeshes.size(), _matCount);
		if (!_submeshes.empty() && ImGui::TreeNode("Submeshes / Materials"))
		{
			for (size_t i = 0; i < _submeshes.size(); ++i)
				ImGui::BulletText("%s  (%u tris, slot %u)",
					_submeshes[i].materialName.c_str(), _submeshes[i].indexCount / 3,
					i < _subMatSlot.size() ? _subMatSlot[i] : 0u);
			ImGui::TreePop();
		}
	}

	if (!_selectedAsset.empty())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Selected Asset", ImGuiTreeNodeFlags_DefaultOpen))
		{
			fs::path p(_selectedAsset);
			ImGui::Text("Name: %s", WToUtf8(p.filename().wstring()).c_str());
			ImGui::Text("Type: %s", WToUtf8(p.extension().wstring()).c_str());
			std::error_code ec; auto sz = fs::file_size(p, ec);
			if (!ec) ImGui::Text("Size: %.1f KB", sz / 1024.0);
		}
	}

	ImGui::End();
}

void D3D12Device::DrawFolderContents()
{
	ImGui::Begin("FolderContents");

	// 경로 바 + 상위 폴더
	bool atRoot = (fs::path(_curDir) == fs::path(_assetRoot));
	if (!atRoot)
	{
		if (ImGui::Button("..")) _curDir = fs::path(_curDir).parent_path().wstring();
		ImGui::SameLine();
	}
	std::wstring rel = fs::relative(_curDir, fs::path(_assetRoot).parent_path()).wstring();
	ImGui::TextDisabled("%s", WToUtf8(rel).c_str());
	ImGui::Separator();

	std::error_code ec;
	std::vector<fs::path> dirs, files;
	for (auto& e : fs::directory_iterator(_curDir, ec))
	{
		if (e.is_directory(ec)) dirs.push_back(e.path());
		else                    files.push_back(e.path());
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());

	for (auto& d : dirs)
	{
		std::string label = "[/] " + WToUtf8(d.filename().wstring());
		if (ImGui::Selectable(label.c_str(), false))
			_curDir = d.wstring();
	}
	for (auto& f : files)
	{
		std::string label = WToUtf8(f.filename().wstring());
		bool sel = (f.wstring() == _selectedAsset);
		if (ImGui::Selectable(label.c_str(), sel))
			_selectedAsset = f.wstring();
	}

	ImGui::End();
}
