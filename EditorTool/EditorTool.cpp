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

#include "MeshThumbnail.h"

void EditorTool::Init()
{
	
	shared_ptr<AsConverter> converter = make_shared<AsConverter>();

//	converter->ReadAssetFile(L"Tower/Tower2.fbx");
//	converter->ExportMaterialData(L"Tower/Tower");
//	converter->ExportModelData(L"Tower/Tower");

	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	_sceneCam = make_shared<SceneCamera>();

	auto shader = RESOURCES->Get<Shader>(L"Standard");

	shared_ptr<GameObject> sceneCamera = make_shared<GameObject>();
	sceneCamera->SetObjectName(L"Scene Camera");
	sceneCamera->GetOrAddTransform()->SetPosition(Vec3{ -31.716f, 14.f, -25.f });
	sceneCamera->AddComponent(make_shared<Camera>());
	sceneCamera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, true);
	sceneCamera->AddComponent(_sceneCam);

	CUR_SCENE->Add(sceneCamera);

	// UI_Camera
	{
		auto uiCamera = make_shared<GameObject>();
		uiCamera->SetObjectName(L"UI Camera");
		uiCamera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -10.f });
		uiCamera->AddComponent(make_shared<Camera>());
		uiCamera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
		uiCamera->GetCamera()->SetNear(1.f);
		uiCamera->GetCamera()->SetFar(100);
		
		uiCamera->GetCamera()->SetCullingMaskAll();
		uiCamera->GetCamera()->SetCullingMaskLayerOnOff(LayerMask::UI, false);

		CUR_SCENE->Add(uiCamera);
	}

	{
	/*	shared_ptr<GameObject> grid = make_shared<GameObject>();
		grid->SetObjectName(L"Scene Grid");
		grid->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, 0.f });
		grid->AddComponent(make_shared<SceneGrid>(MAIN_CAM));
		CUR_SCENE->Add(grid);*/
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



/*	auto collider = make_shared<OBBBoxCollider>();
		collider->GetBoundingBox().Extents = Vec3(1.f);
		obj->AddComponent(collider);*/

	//	for (int i = 100; i < 110; i++)
	//	{
	//		auto obj = make_shared<GameObject>();
	//		wstring name = L"Model_" + to_wstring(i);
	//		obj->SetObjectName(name);

	//		obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
	//		obj->GetOrAddTransform()->SetScale(Vec3(0.1f));

	//		obj->AddComponent(make_shared<ModelRenderer>(shader));
	//		obj->GetModelRenderer()->SetModel(m2);
	//		obj->GetModelRenderer()->SetPass(1);

	///*	auto collider = make_shared<OBBBoxCollider>();
	//		collider->GetBoundingBox().Extents = Vec3(1.f);
	//		obj->AddComponent(collider);*/

	//		CUR_SCENE->Add(obj);
	//	}
	//}
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

	//{
	//	shared_ptr<class Model> m2 = make_shared<Model>();
	//	m2->ReadModel(L"Juno/Juno");
	//	m2->ReadMaterial(L"Juno/Juno");

	//	for (int i = 10; i < 15; i++)
	//	{
	//		auto obj = make_shared<GameObject>();
	//		wstring name = L"Model_" + to_wstring(i);
	//		obj->SetObjectName(name);

	//		if(i == 10)
	//			obj->GetOrAddTransform()->SetPosition(Vec3(3,0,5));
	//		else
	//			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));

	//		obj->GetOrAddTransform()->SetScale(Vec3(10.f));

	//		obj->AddComponent(make_shared<ModelRenderer>(shader));
	//		obj->GetModelRenderer()->SetModel(m2);
	//		obj->GetModelRenderer()->SetPass(1);

	//	//	auto collider = make_shared<OBBBoxCollider>();
	//	//	collider->SetOffset(Vec3(0.f, 8.f, 0.f));
	//	//	collider->GetBoundingBox().Extents = Vec3(1.5f, 2.8f, 1.5f);		
	//	//	obj->AddComponent(collider);

	//		CUR_SCENE->Add(obj);
	//	}
	//}
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

	//ADDLOG("Test!!" , LogFilter::Info);
}

void EditorTool::Render()
{
	
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
