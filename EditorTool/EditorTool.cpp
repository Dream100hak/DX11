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
#include "SnowBillboard.h"
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
#include <boost/describe.hpp>
#include <boost/mp11.hpp>

#include "AsConverter.h"

void EditorTool::Init()
{
	
	shared_ptr<AsConverter> converter = make_shared<AsConverter>();

	/* converter->ReadAssetFile(L"Hyejin/Hyejin_S002_LOD1.fbx");
	 converter->ExportMaterialData(L"Hyejin/Hyejin");
     converter->ExportModelData(L"Hyejin/Hyejin");*/

	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	_sceneCam = make_shared<SceneCamera>();

	auto shader = RESOURCES->Get<Shader>(L"Standard");

	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Scene Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ -31.716f, 14.f, -25.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, true);
		camera->AddComponent(_sceneCam);

		CUR_SCENE->Add(camera);
	}
	// UI_Camera
	{
		auto camera = make_shared<GameObject>();
		camera->SetObjectName(L"UI Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -10.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
		camera->GetCamera()->SetNear(1.f);
		camera->GetCamera()->SetFar(100);
	
		camera->GetCamera()->SetCullingMaskAll();
		camera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, false);

		CUR_SCENE->Add(camera);
	}

	{
		shared_ptr<GameObject> grid = make_shared<GameObject>();
		grid->SetObjectName(L"Scene Grid");
		grid->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, 0.f });
		grid->AddComponent(make_shared<SceneGrid>());
		CUR_SCENE->Add(grid);
	}
	
	{
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
		
		//// Sky
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"SkyBox");
		obj->GetOrAddTransform();
		obj->AddComponent(make_shared<SkyBox>());
		obj->GetSkyBox()->Init(SkyType::CubeMap);
		CUR_SCENE->Add(obj);
		
	}

	{
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"Terrain");
		obj->GetOrAddTransform();
		obj->GetOrAddTransform()->SetPosition(Vec3(-75.f, 0.f, -75.f));
		obj->AddComponent(make_shared<Terrain>());

		auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");
		obj->GetTerrain()->Create(200, 200, mat->Clone());
		CUR_SCENE->Add(obj);
	}


	 //Model
	{

		shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Kachujin/Kachujin");
		m2->ReadMaterial(L"Kachujin/Kachujin");

		for (int i = 100; i < 110; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Model_" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.1f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);

	/*	auto collider = make_shared<OBBBoxCollider>();
			collider->GetBoundingBox().Extents = Vec3(1.f);
			obj->AddComponent(collider);*/

			CUR_SCENE->Add(obj);
		}
	}
	// Model
	//{

	//	shared_ptr<class Model> m2 = make_shared<Model>();
	//	m2->ReadModel(L"Kachujin/Kachujin");
	//	m2->ReadMaterial(L"Kachujin/Kachujin");
	//	m2->ReadAnimation(L"Kachujin/Idle");
	//	m2->ReadAnimation(L"Kachujin/Run");
	//	m2->ReadAnimation(L"Kachujin/Slash");

	//	for (int i = 130; i < 140; i++)
	//	{
	//		auto obj = make_shared<GameObject>();
	//		wstring name = L"Model_" + to_wstring(i);
	//		obj->SetObjectName(name);

	//		obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
	//		obj->GetOrAddTransform()->SetScale(Vec3(0.1f));

	//		obj->AddComponent(make_shared<ModelAnimator>(shader));
	//		obj->GetModelAnimator()->SetModel(m2);
	//		obj->GetModelAnimator()->SetPass(2);

	//		auto collider = make_shared<OBBBoxCollider>();
	//		collider->GetBoundingBox().Extents = Vec3(1.f);
	//		obj->AddComponent(collider);

	//		CUR_SCENE->Add(obj);
	//	}
	//}
	//{

	{
		shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Juno/Juno");
		m2->ReadMaterial(L"Juno/Juno");

		for (int i = 10; i < 10; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Model_" + to_wstring(i);
			obj->SetObjectName(name);

			if(i == 10)
				obj->GetOrAddTransform()->SetPosition(Vec3(3,0,5));
			else
				obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));

			obj->GetOrAddTransform()->SetScale(Vec3(10.f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);

		//	auto collider = make_shared<OBBBoxCollider>();
		//	collider->SetOffset(Vec3(0.f, 8.f, 0.f));
		//	collider->GetBoundingBox().Extents = Vec3(1.5f, 2.8f, 1.5f);		
		//	obj->AddComponent(collider);

			CUR_SCENE->Add(obj);
		}
	}
}

void EditorTool::Update()
{
	GET_SINGLE(ShortcutManager)->Update();
	GET_SINGLE(EditorToolManager)->Update();

	ImGui::ShowDemoWindow(&_showWindow);

	auto shadowMap = GRAPHICS->GetShadowMap();
	auto shadowTex = static_pointer_cast<Texture>(shadowMap);

	ImGui::Begin("ShadowMap");
	ImGui::Image((void*)shadowMap->GetComPtr().Get(), ImVec2(400, 400));
	ImGui::End();
}

void EditorTool::Render()
{
	
}

void EditorTool::OnMouseWheel(int32 scrollAmount)
{
	_sceneCam->MoveCam(scrollAmount);
}
