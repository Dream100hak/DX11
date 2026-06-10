#include "pch.h"
#include "ShadowMap.h"
#include "EditorTool.h"
#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "SceneCamera.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Terrain.h"
#include "Billboard.h"

#include "Button.h"
#include "OBBBoxCollider.h"
#include "SkyBox.h"
#include "Utils.h"
#include "SceneGrid.h"

#include "LogWindow.h"
#include "MathUtils.h"

#include "Material.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "TextureManager.h"

#include "AsConverter.h"

#include "MeshThumbnail.h"


#include "Ssao.h"
#include "ParticleSystem.h"
#include "SkyCubeMap.h"

#include "Hiearchy.h"
#include "SceneWindow.h"

void EditorTool::Init()
{

	//{
	//	shared_ptr<AsConverter> converter = make_shared<AsConverter>();

	//	converter->ReadAssetFile(L"Kachujin/Mesh.fbx");
	/////	converter->ExportMaterialDataByXml(L"Kachujin/Kachujin");
	//	converter->ExportMaterialDataByMats(L"Kachujin/Kachujin");
	//	converter->ExportModelData(L"Kachujin/Kachujin");

	//}
	//{
	//	shared_ptr<AsConverter> converter = make_shared<AsConverter>();
	//	converter->ReadAssetFile(L"Kachujin/Idle.fbx");
	//	converter->ExportAnimationData(L"Kachujin/Idle");
	//}

	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	_sceneCam = make_shared<SceneCamera>();

	shared_ptr<GameObject> sceneCamera = make_shared<GameObject>();
	sceneCamera->SetObjectName(L"Game Camera");
	sceneCamera->GetOrAddTransform()->SetPosition(Vec3{ 181.f, 26.521f, -25.599f });
	sceneCamera->GetOrAddTransform()->SetRotation(Vec3{ 0.381f, -0.784f, 0.f });

	//sceneCamera->GetOrAddTransform()->SetPosition(Vec3(-1.5f, 1.f, -4.f));
	//sceneCamera->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.35f, 0.f));

	sceneCamera->AddComponent(make_shared<Camera>());
	sceneCamera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, true);
	sceneCamera->AddComponent(_sceneCam);
	CUR_SCENE->Add(sceneCamera);

	GET_SINGLE(TextureManager)->Init();

	{
		//auto simpleGrid = make_shared<GameObject>();
		//simpleGrid->SetObjectName(L"grid");
		//simpleGrid->AddComponent(make_shared<SimpleGrid>());
		//simpleGrid->GetOrAddTransform()->SetPosition(Vec3(-10.f, 0, -10.f));
		//auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");
		//mat->GetMaterialDesc().useTexture = false;
		//simpleGrid->GetComponent<SimpleGrid>()->Create(50, 50, mat->Clone());
		//CUR_SCENE->Add(simpleGrid);
	}
	{

		/*shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Kachujin/Kachujin");
		m2->ReadMaterial(L"Kachujin/Kachujin");

		int32 cnt = 1;
		
		auto obj = make_shared<GameObject>();
		wstring name = L"Ani_" + to_wstring(cnt++);
		obj->SetObjectName(name);

		obj->AddComponent(make_shared<ModelRenderer>(shader));
		BoundingBox box = m2->CalculateModelBoundingBox();

		float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
		float globalScale = MODEL_GLOBAL_SCALE;

		if (modelScale > 10.f)
			modelScale = globalScale;

		float scale = (globalScale / modelScale) * 0.1f;

		obj->GetOrAddTransform()->SetPosition(Vec3(150.f, 0.f, -20.f));
		obj->GetOrAddTransform()->SetRotation(Vec3::Zero);
		obj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

		obj->AddComponent(make_shared<ModelRenderer>(shader));
		obj->GetModelRenderer()->SetModel(m2);
		obj->GetModelRenderer()->SetPass(1);

		CUR_SCENE->Add(obj);*/
	}

	shared_ptr<Hiearchy> hieachy = static_pointer_cast<Hiearchy>(TOOL->GetEditorWindow(Utils::GetClassNameEX<Hiearchy>()));

	// UI_Camera
	{
		/*	auto uiCamera = make_shared<GameObject>();
			uiCamera->SetObjectName(L"UI Camera");
			uiCamera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -10.f });
			uiCamera->AddComponent(make_shared<Camera>());
			uiCamera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
			uiCamera->GetCamera()->SetNear(1.f);
			uiCamera->GetCamera()->SetFar(100);

			uiCamera->GetCamera()->SetCullingMaskAll();
			uiCamera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, false);

			CUR_SCENE->Add(uiCamera);*/
	}

	{
		//// Light
		auto light = make_shared<GameObject>();
		light->SetObjectName(L"Direction Light");
		light->GetOrAddTransform()->SetRotation(Vec3(-0.57735f, -0.57735f, 0.57735f));
		light->AddComponent(make_shared<Light>());
		LightDesc lightDesc;

		lightDesc.ambient = Vec4(0.7f, 0.7f, 0.7f, 1.0f);
		lightDesc.diffuse = Vec4(0.8f, 0.8f, 0.7f, 1.0f);
		lightDesc.specular = Vec4(0.8f, 0.8f, 0.7f, 1.0f);
		lightDesc.direction = light->GetTransform()->GetRotation();
		light->GetLight()->SetLightDesc(lightDesc);
		CUR_SCENE->Add(light);
	}

	{
		//SKY
		hieachy->CreateSky();
	}

	// Model

		auto terrainObj = make_shared<GameObject>();
		terrainObj->SetObjectName(L"Terrain");
		terrainObj->AddComponent(make_shared<Terrain>());

		TerrainInfo info{};

		info.heightMapFilename = L"../Resources/Assets/Textures/Terrain/terrain.raw";
		info.blendMapFilename = L"../Resources/Assets/Textures/Terrain/blend.dds";
		info.layerMapFilenames.push_back(L"../Resources/Assets/Textures/Terrain/grass.dds");
		info.layerMapFilenames.push_back(L"../Resources/Assets/Textures/Terrain/darkdirt.dds");
		info.layerMapFilenames.push_back(L"../Resources/Assets/Textures/Terrain/stone.dds");
		info.layerMapFilenames.push_back(L"../Resources/Assets/Textures/Terrain/lightdirt.dds");
		info.layerMapFilenames.push_back(L"../Resources/Assets/Textures/Terrain/snow.dds");

		info.heightScale = 50.0f;
		info.heightmapWidth = 2049;
		info.heightmapHeight = 2049;
		info.cellSpacing = 0.5f;

		auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");

		shared_ptr<Material> matClone = mat->Clone();
		matClone->GetMaterialDesc().roughness = 0.9f; // 지면 — 거의 무광
		//matClone->SetShadowMap(static_pointer_cast<Texture>(shadow));
		terrainObj->GetComponent<Terrain>()->Init(info, matClone);

		CUR_SCENE->Add(terrainObj);

		//shared_ptr<class Model> m2 = make_shared<Model>();
		//m2->ReadModel(L"Kachujin/Kachujin");
		//m2->ReadMaterial(L"Kachujin/Kachujin");

		//for (int i = 130; i < 140; i++)
		//{
		//	auto obj = make_shared<GameObject>();
		//	wstring name = L"Model_" + to_wstring(i);
		//	obj->SetObjectName(name);

		//	float randX = MathUtils::Random(100.f,200.f);
		//	float randZ = MathUtils::Random(-20.f, 20.f);
		//	float y = 0.f;

		//	if (terrainObj->GetComponent<Terrain>() != nullptr)
		//	{
		//		y = terrainObj->GetComponent<Terrain>()->GetHeight(randX, randZ);
		//	}

		//	obj->GetOrAddTransform()->SetPosition(Vec3(randX, y, randZ));
		//	obj->GetOrAddTransform()->SetScale(Vec3(6.0f));

		//	obj->AddComponent(make_shared<ModelRenderer>(shader));
		//	obj->GetModelRenderer()->SetModel(m2);
		//	obj->GetModelRenderer()->SetPass(1);

		//	CUR_SCENE->Add(obj);
		//}
	 //}
		//{

		//	shared_ptr<class model> m2 = make_shared<model>();
		//	m2->readmodel(l"kachujin/kachujin");
		//	m2->readmaterial(l"kachujin/kachujin");
		//	m2->readanimation(l"kachujin/idle");
		//	m2->readanimation(l"kachujin/run");
		//	m2->readanimation(l"kachujin/slash");

		//	for (int i = 200; i < 201; i++)
		//	{
		//		auto obj = make_shared<gameobject>();
		//		wstring name = l"ani_" + to_wstring(i);
		//		obj->setobjectname(name);

		//		float randx = mathutils::random(100.f, 150.f);
		//		float randz = mathutils::random(-20.f, 20.f);

		//		obj->getoraddtransform()->setposition(vec3(randx, 0, randz));
		//		//obj->getoraddtransform()->setscale(vec3(0.1f));

		//		obj->addcomponent(make_shared<modelanimator>(shader));
		//		obj->getmodelanimator()->setmodel(m2);
		//		obj->getmodelanimator()->setpass(2);

		//		cur_scene->add(obj);
		//	}
		//}
	
}

void EditorTool::Update()
{
	// ✅ 게임 로직 업데이트
	SCENE->Update();
	
	GET_SINGLE(ShortcutManager)->Update();
	GET_SINGLE(EditorToolManager)->Update();
	GET_SINGLE(TextureManager)->Update();

	ImGui::ShowDemoWindow(&_showWindow);

	DrawRenderTextures();
}

void EditorTool::Render()
{
	// 렌더링은 SceneManager::Update() → Scene::Render() → Camera::Render_Forward()에서
	// 메인 백버퍼에 직접 처리됨
	// (ImGui "Scene" 윈도우가 NoBackground 플래그로 백버퍼를 그대로 보여주는 구조)
}

void EditorTool::OnMouseWheel(int32 scrollAmount)
{
	int32 x = INPUT->GetMousePos().x;
	int32 y = INPUT->GetMousePos().y;

	if (GRAPHICS->IsMouseInViewport(x, y))
	{
		_sceneCam->MoveCam(scrollAmount);
	}
}

void EditorTool::DrawRenderTextures()
{
	auto tex1 = TEXTURE->GetShadowMap()->GetComPtr().Get();
	auto tex2 = TEXTURE->GetSsao()->GetAmbientPtr().Get();
	auto tex3 = TEXTURE->GetSsao()->GetNormalDepthPtr().Get();
//	auto tex4 = TEXTURE->GetTerrain()->GetLayerSRV().Get();

	ImGui::Begin("RenderTextures");
	ImGui::Image(tex1, ImVec2(200, 150));
	ImGui::Text("shadow");
	ImGui::Image(tex2, ImVec2(200, 150));
	ImGui::Text("ssao");
	ImGui::Image(tex3, ImVec2(200, 150));
	ImGui::Text("depthNormal");
	ImGui::End();
}
