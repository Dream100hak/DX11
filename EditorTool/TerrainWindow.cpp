#include "pch.h"
#include "TerrainWindow.h"
#include "Terrain.h"
#include "GameObject.h"

bool  TerrainWindow::S_Edit = false;
int32 TerrainWindow::S_Mode = 0;
float TerrainWindow::S_Radius = 8.0f;
float TerrainWindow::S_Strength = 0.30f;

TerrainWindow::TerrainWindow(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
}

TerrainWindow::~TerrainWindow()
{
}

void TerrainWindow::Init()
{
}

void TerrainWindow::Update()
{
	ImGui::Begin("Terrain");

	auto scene = CUR_SCENE;
	shared_ptr<GameObject> terrainObj = scene ? scene->GetTerrain() : nullptr;
	shared_ptr<Terrain> terrain = terrainObj ? terrainObj->GetTerrain() : nullptr;

	if (terrain == nullptr)
	{
		// 씬에 터레인이 없으면 편집 불가
		ImGui::TextWrapped("No terrain in scene. Load a .scene or create one first.");
		ImGui::End();
		return;
	}

	const TerrainInfo& info = terrain->GetInfo();

	// 편집 모드: 켜지면 씬뷰 좌클릭이 픽킹 대신 스컬프팅
	ImGui::Checkbox("Enable Editing", &S_Edit);
	if (S_Edit)
		ImGui::TextDisabled("LMB-drag in Scene view to sculpt (picking/gizmo disabled)");

	ImGui::SeparatorText("Brush");
	const char* modes[] = { "Raise", "Lower", "Smooth", "Flatten" };
	ImGui::Combo("Mode", &S_Mode, modes, IM_ARRAYSIZE(modes));
	ImGui::DragFloat("Radius", &S_Radius, 0.2f, 0.5f, 300.f);
	ImGui::DragFloat("Strength", &S_Strength, 0.005f, 0.f, 5.f);

	ImGui::SeparatorText("Terrain Info");
	ImGui::Text("Heightmap : %u x %u", info.heightmapWidth, info.heightmapHeight);
	ImGui::Text("Cell Spacing : %.3f", info.cellSpacing);
	ImGui::Text("World Size : %.1f x %.1f", terrain->GetWorldWidth(), terrain->GetWorldDepth());

	ImGui::SeparatorText("Utilities");
	static float flatHeight = 0.f;
	ImGui::DragFloat("Flat Height", &flatHeight, 0.1f);
	if (ImGui::Button("Flatten All"))
		terrain->FlattenAll(flatHeight);

	ImGui::End();
}
