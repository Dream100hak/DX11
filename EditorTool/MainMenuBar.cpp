#include "pch.h"
#include "MainMenuBar.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"

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

void MainMenuBar::MenuFileList()
{
	if (ImGui::MenuItem("New")) {}
	if (ImGui::MenuItem("Open", "Ctrl+O")) {}
	if (ImGui::MenuItem("Save", "Ctrl+S")) {}
	if (ImGui::MenuItem("Save As..")) {}

	ImGui::Separator();
	if (ImGui::BeginMenu("Options"))
	{
		static bool enabled = true;
		ImGui::MenuItem("Enabled", "", &enabled);
		ImGui::BeginChild("child", ImVec2(0, 60), true);
		for (int i = 0; i < 10; i++)
			ImGui::Text("Scrolling Text %d", i);
		ImGui::EndChild();
		static float f = 0.5f;
		static int n = 0;
		ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
		ImGui::InputFloat("Input", &f, 0.1f);
		ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Colors"))
	{
		float sz = ImGui::GetTextLineHeight();
		for (int i = 0; i < ImGuiCol_COUNT; i++)
		{
			const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
			ImGui::Dummy(ImVec2(sz, sz));
			ImGui::SameLine();
			ImGui::MenuItem(name);
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Options")) // <-- Append!
	{
		static bool b = true;
		ImGui::Checkbox("SomeOption", &b);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Disabled", false)) // Disabled
	{
		IM_ASSERT(0);
	}
	if (ImGui::MenuItem("Checked", NULL, true)) {}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4")) {}
}

void MainMenuBar::AppPlayMenu()
{
	ImGui::SetNextWindowPos(ImVec2(800, 21));
	ImGui::SetNextWindowSize(ImVec2(1920 - 800, 30));
	ImGui::Begin("PlayMenu", NULL, ImGuiCol_FrameBg);

	ImGui::SetCursorPos(ImVec2((1920 - 800) * 0.5f, ImGui::GetCursorPosY()));
	if (ImGui::Button("Play", ImVec2(50, 0)))
	{

	}

	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(50, 0)))
	{

	}
	ImGui::End();
}
