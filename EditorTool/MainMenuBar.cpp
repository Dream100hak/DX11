#include "pch.h"
#include "MainMenuBar.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"
#include "UfbxConverter.h"
#include "SceneSerializer.h"
#include "Utils.h"
#include <filesystem>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

namespace
{
	// 씬 저장 다이얼로그 — 기본 폴더 Resources/Assets/Scenes
	void SaveSceneDialog()
	{
		std::filesystem::path sceneDir = std::filesystem::absolute(L"../Resources/Assets/Scenes");
		std::filesystem::create_directories(sceneDir);

		wchar_t szFile[MAX_PATH] = L"NewScene";
		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = L"Scene Files (*.scene)\0*.scene\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = sceneDir.c_str();
		ofn.lpstrDefExt = L"scene";
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		if (::GetSaveFileNameW(&ofn) == FALSE)
			return;

		SceneSerializer::Save(szFile);
	}

	void OpenSceneDialog()
	{
		std::filesystem::path sceneDir = std::filesystem::absolute(L"../Resources/Assets/Scenes");

		wchar_t szFile[MAX_PATH] = L"";
		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = L"Scene Files (*.scene)\0*.scene\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = sceneDir.c_str();
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (::GetOpenFileNameW(&ofn) == FALSE)
			return;

		SceneSerializer::Load(szFile);
	}
}


MainMenuBar::MainMenuBar()
{

}

MainMenuBar::~MainMenuBar()
{

}

void MainMenuBar::Init()
{

}

void MainMenuBar::Update()
{
	ShowMainMenuBar();
	AppPlayMenu();
}

void MainMenuBar::ShowMainMenuBar()
{

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			MenuFileList();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("GameObject"))
		{
			if (ImGui::MenuItem("Create Empty", "CTRL+B")) { TOOL->SetSelectedObjH(GUI->CreateEmptyGameObject());  ADDLOG("Create GameObject", LogFilter::Info); }
			if (ImGui::MenuItem("Create Empty Child", "CTRL+Z")) {}
			if (ImGui::MenuItem("Create Empty Parent", "CTRL+Z")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("2D Object", "CTRL+Z")) {}
			if (ImGui::MenuItem("3D Object", "CTRL+Z")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Component"))
		{
			if (ImGui::MenuItem("Mesh", "CTRL+Z")) {}
			if (ImGui::MenuItem("Sound", "CTRL+Z")) {}
			if (ImGui::MenuItem("Light", "CTRL+Z")) {}
			if (ImGui::MenuItem("Camera", "CTRL+Z")) {}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

// FBX 선택 다이얼로그 -> ufbx 변환
// 출력 규칙: Models/<FBX 부모 폴더명>/ 에 모델(.mesh/.mmat/.mat) + 클립(<파일스템>.clip)
void MainMenuBar::ConvertFbx()
{
	wchar_t szFile[MAX_PATH] = L"";
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"FBX Files (*.fbx)\0*.fbx\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (::GetOpenFileNameW(&ofn) == FALSE)
		return;

	auto fbxPath = std::filesystem::path(szFile);
	wstring folderName = fbxPath.parent_path().filename().wstring();
	wstring stem = fbxPath.stem().wstring();

	shared_ptr<UfbxConverter> conv = make_shared<UfbxConverter>();
	conv->ReadAssetFile(fbxPath.wstring());

	if (conv->HasMesh())
	{
		conv->ExportMaterialDataByMats(folderName + L"/" + folderName);
		conv->ExportModelData(folderName + L"/" + folderName);
		ADDLOG("Convert FBX -> .mesh/.mmat : " + Utils::ToString(folderName), LogFilter::Info);
	}

	if (conv->GetAnimationCount() > 0)
	{
		conv->ExportAnimationData(folderName + L"/" + stem);
		ADDLOG("Convert FBX -> .clip : " + Utils::ToString(folderName + L"/" + stem), LogFilter::Info);
	}

	if (conv->HasMesh() == false && conv->GetAnimationCount() == 0)
		ADDLOG("Convert FBX : no mesh/animation found", LogFilter::Warn);
}

void MainMenuBar::MenuFileList()
{
	if (ImGui::MenuItem("New Scene")) { SceneSerializer::Clear(); ADDLOG("New Scene", LogFilter::Info); }
	if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { OpenSceneDialog(); }
	if (ImGui::MenuItem("Save Scene...", "Ctrl+S")) { SaveSceneDialog(); }

	ImGui::Separator();
	if (ImGui::MenuItem("Convert FBX...")) { ConvertFbx(); }

	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4"))
		::PostMessage(GAME->GetGameDesc().hWnd, WM_CLOSE, 0, 0);
}

void MainMenuBar::AppPlayMenu()
{
	// 씬뷰 상단 중앙 플로팅 오버레이 (PassViewer 와 같은 방식) — 플레이 모드 구현 전까지 자리만
	const SceneDesc& scene = GAME->GetSceneDesc();
	const ImGuiStyle& style = ImGui::GetStyle();

	const float btnW = 50.f;
	const float barW = btnW * 2 + style.ItemSpacing.x + style.WindowPadding.x * 2;

	ImGui::SetNextWindowPos(ImVec2(scene.x + (scene.width - barW) * 0.5f, scene.y + 8.f));
	ImGui::SetNextWindowBgAlpha(0.6f);
	ImGui::Begin("PlayMenu", NULL,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

	const bool playing = TOOL->IsPlaying();

	// 플레이 중 — Play 버튼 액센트 강조
	if (playing)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.52f, 0.88f, 1.f));

	if (ImGui::Button("Play", ImVec2(btnW, 0)))
		TOOL->StartPlay();

	if (playing)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(btnW, 0)))
		TOOL->StopPlay();

	if (playing)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.36f, 0.65f, 1.f, 1.f), "PLAYING");
	}

	ImGui::End();
}
