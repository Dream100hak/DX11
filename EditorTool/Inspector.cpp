#include "pch.h"
#include "Inspector.h"
#include "EditorToolManager.h"

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "MeshThumbnail.h"

#include "SimpleGrid.h"
#include "SceneGrid.h"
#include "SkyCubeMap.h"

// -----------------------------------------------------------
// Inspector — 공통 진입점 (윈도우/프리뷰 리소스 초기화 + 모드 분기)
//  - 하이어라키 모드: InspectorHierarchy.cpp
//  - 프로젝트 모드:   InspectorProject.cpp
// -----------------------------------------------------------

Inspector::Inspector(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
}

Inspector::~Inspector()
{
}

void Inspector::Init()
{
	RESOURCES->Load<Texture>(L"Grid", L"..\\Resources\\Assets\\Textures\\Grid.png");
	RESOURCES->Load<Texture>(L"ObjIcon", L"..\\Resources\\Assets\\Textures\\Obj.png");

	if (_meshPreviewCamera == nullptr)
	{
		_meshPreviewCamera = make_shared<GameObject>();
		_meshPreviewCamera->AddComponent(make_shared<Camera>());
		_meshPreviewCamera->GetOrAddTransform()->SetPosition(Vec3(-1.5f, 1.f, -4.f));
		_meshPreviewCamera->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.35f, 0.f));
		_meshPreviewCamera->GetCamera()->UpdateMatrix();
		_meshPreviewCamera->GetCamera()->SetFOV(0.65f);
	}

	if (_meshPreviewLight == nullptr)
	{
		_meshPreviewLight = make_shared<GameObject>();
		_meshPreviewLight->GetOrAddTransform()->SetRotation(Vec3(-0.57735f, -0.57735f, 0.57735f));
		_meshPreviewLight->AddComponent(make_shared<Light>());
		LightDesc lightDesc;

		lightDesc.ambient = Vec4(1.f, 1.0f, 1.0f, 1.0f);
		lightDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightDesc.specular = Vec4(0.8f, 0.8f, 0.7f, 1.0f);
		lightDesc.direction = _meshPreviewLight->GetTransform()->GetRotation();
		_meshPreviewLight->GetLight()->SetLightDesc(lightDesc);
	}

	if (_simpleGrid == nullptr)
	{
		_simpleGrid = make_shared<GameObject>();
		_simpleGrid->AddComponent(make_shared<SimpleGrid>());
		_simpleGrid->GetOrAddTransform()->SetPosition(Vec3(-10.f, -0.5f, -10.f));
		auto mat = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();
		mat->GetMaterialDesc().useTexture = false;
		mat->GetMaterialDesc().diffuse = Color(0.1f, 0.1f, 0.1f, 0.5f);
		_simpleGrid->GetComponent<SimpleGrid>()->Create(50, 50, mat->Clone());

	}

	if (_sceneGrids.size() == 0)
	{
		vector<pair<int32,float>> gridSamples = { {100, 5} , {100,3} ,{100,2} };

		for (int32 i = 0; i < 3; i++)
		{
			shared_ptr<GameObject> sceneGrid = make_shared<GameObject>();
			sceneGrid->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.001f, 0.f });
			sceneGrid->GetOrAddTransform()->SetRotation(Vec3{ 0.f, 0.25f, 0.f });
			sceneGrid->AddComponent(make_shared<SceneGrid>());

			int32 gridCount = gridSamples[i].first;
			float gridSize = gridSamples[i].second;

			sceneGrid->GetComponent<SceneGrid>()->Init(gridCount, gridSize );
			_sceneGrids.push_back(sceneGrid);
		}
	}

	if (_skyBox == nullptr)
	{
		_skyBox = make_shared<GameObject>();
		_skyBox->GetOrAddTransform()->SetPosition(Vec3::Zero);
		_skyBox->AddComponent(make_shared<SkyCubeMap>());
		_skyBox->GetComponent<SkyCubeMap>()->Init(L"../Resources/Assets/Textures/desertcube1024.dds");
	}


}

void Inspector::Update()
{
	ShowInspector();
}

void Inspector::ShowInspector()
{
	ImGui::Begin("Inspector"); // 위치/크기는 도크가 결정

	//하이어라키 설정
	shared_ptr<MetaData> metaData = SELECTED_P;

	if (SELECTED_H > -1)
	{
		ShowInfoHiearchy();
	}
	else if (metaData != nullptr && metaData->metaType != NONE)
	{
		ShowInfoProject();
	}

	ImGui::End();
}
