#include "pch.h"
#include "TerrainWindow.h"
#include "Terrain.h"
#include "Foliage.h"
#include "GameObject.h"
#include "Utils.h"
#include "Define.h"
#include "LogWindow.h"
#include <filesystem>
#include <string>

bool  TerrainWindow::S_Edit = false;
int32 TerrainWindow::S_Tool = 0;
int32 TerrainWindow::S_Mode = 0;
int32 TerrainWindow::S_Layer = 1;
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

	// 편집 모드: 켜지면 씬뷰 좌클릭이 픽킹 대신 브러시
	ImGui::Checkbox("Enable Editing", &S_Edit);
	if (S_Edit)
		ImGui::TextDisabled("LMB-drag in Scene view (picking/gizmo disabled)");

	ImGui::SeparatorText("Tool");
	ImGui::RadioButton("Sculpt", &S_Tool, 0); ImGui::SameLine();
	ImGui::RadioButton("Paint", &S_Tool, 1);

	if (S_Tool == 0) // ── Sculpt ──
	{
		const char* modes[] = { "Raise", "Lower", "Smooth", "Flatten" };
		ImGui::Combo("Mode", &S_Mode, modes, IM_ARRAYSIZE(modes));
	}
	else // ── Paint ──
	{
		// 레이어 0(베이스) ~ 4. 라벨에 텍스처 파일명 스템 표시
		std::string labels[5];
		for (int32 i = 0; i < 5; ++i)
		{
			std::string name = (i < (int32)info.layerMapFilenames.size())
				? Utils::ToString(std::filesystem::path(info.layerMapFilenames[i]).stem().wstring())
				: "(none)";
			labels[i] = std::to_string(i) + " : " + name;
		}
		const char* items[5] = { labels[0].c_str(), labels[1].c_str(), labels[2].c_str(), labels[3].c_str(), labels[4].c_str() };
		ImGui::Combo("Layer", &S_Layer, items, 5);
		ImGui::TextDisabled("Strength=1 paints solid; lower blends gradually");
	}

	ImGui::DragFloat("Radius", &S_Radius, 0.2f, 0.5f, 300.f);
	ImGui::DragFloat("Strength", &S_Strength, 0.005f, 0.f, 1.f);

	ImGui::SeparatorText("Terrain Info");
	ImGui::Text("Heightmap : %u x %u", info.heightmapWidth, info.heightmapHeight);
	ImGui::Text("Cell Spacing : %.3f", info.cellSpacing);
	ImGui::Text("World Size : %.1f x %.1f", terrain->GetWorldWidth(), terrain->GetWorldDepth());

	ImGui::SeparatorText("Utilities");
	static float flatHeight = 0.f;
	ImGui::DragFloat("Flat Height", &flatHeight, 0.1f);
	if (ImGui::Button("Flatten All"))
		terrain->FlattenAll(flatHeight);

	ImGui::SeparatorText("Save / Load");
	if (ImGui::Button("Save Terrain Files"))
	{
		if (terrain->SaveEditedTerrain())
			ADDLOG("Terrain saved (*_edit.r32 + *_edit.dds). Now File > Save Scene to persist.", LogFilter::Info);
		else
			ADDLOG("Terrain save failed", LogFilter::Warn);
	}
	ImGui::TextDisabled("Writes height(.r32)/blend(.dds), updates paths.\nThen File > Save Scene to keep edits.");

	ImGui::SeparatorText("Foliage (Grass)");
	static int32 grassCount = 4000;
	static float grassW = 0.6f, grassH = 1.0f;
	static int32 densityLayer = 0; // 0 = Uniform, 1..5 = 블렌드 레이어 0..4
	ImGui::DragInt("Count", &grassCount, 50.f, 0, 300000);
	ImGui::DragFloat("Blade Width", &grassW, 0.01f, 0.05f, 5.f);
	ImGui::DragFloat("Blade Height", &grassH, 0.01f, 0.05f, 10.f);
	// 밀도 소스: Uniform(균일) 또는 칠한 블렌드 레이어 가중치 비례. 라벨에 실제 텍스처명 표시.
	std::string densLabels[6];
	densLabels[0] = "Uniform";
	for (int32 i = 0; i < 5; ++i)
	{
		std::string nm = (i < (int32)info.layerMapFilenames.size())
			? Utils::ToString(std::filesystem::path(info.layerMapFilenames[i]).stem().wstring())
			: "(none)";
		densLabels[i + 1] = "Layer " + std::to_string(i) + " : " + nm;
	}
	const char* densItems[6] = { densLabels[0].c_str(), densLabels[1].c_str(), densLabels[2].c_str(),
		densLabels[3].c_str(), densLabels[4].c_str(), densLabels[5].c_str() };
	ImGui::Combo("Density By", &densityLayer, densItems, IM_ARRAYSIZE(densItems));
	if (ImGui::Button("Generate Grass"))
		terrain->GenerateFoliage(grassCount, grassW, grassH, densityLayer - 1); // -1 = Uniform
	ImGui::SameLine();
	if (ImGui::Button("Clear Grass"))
		terrain->ClearFoliage();

	if (auto fo = terrain->GetFoliage())
	{
		ImGui::Text("Instances: %d", fo->GetCount());
		ImGui::Text("Chunks visible: %d / %d", fo->GetVisibleChunks(), fo->GetChunkCount());
		ImGui::DragFloat("Wind Strength", &fo->Params().WindStrength, 0.005f, 0.f, 3.f);
		ImGui::DragFloat("Wind Freq", &fo->Params().WindFreq, 0.01f, 0.f, 10.f);
		ImGui::DragFloat("View Distance", &fo->Params().MaxDist, 1.f, 5.f, 2000.f);
		ImGui::DragFloat("Fade Range", &fo->Params().FadeRange, 0.5f, 1.f, 500.f);
	}

	ImGui::End();
}
