#include "pch.h"
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

#include "Material.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include <boost/describe.hpp>
#include <boost/mp11.hpp>

void EditorTool::Init()
{

	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	auto shader = RESOURCES->Get<Shader>(L"Standard");

	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Scene Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		_sceneCam = make_shared<SceneCamera>();
	
		camera->AddComponent(_sceneCam);
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
		light->GetOrAddTransform()->SetPosition(Vec3(0.f));
		light->GetOrAddTransform()->SetRotation(Vec3(0.5f, 0.3f, 1.f));
		light->AddComponent(make_shared<Light>());
		LightDesc lightDesc;
		lightDesc.ambient = Vec4(1.f);
		lightDesc.diffuse = Vec4(1.f, 1.f , 1.f, 1.f);
		lightDesc.specular = Vec4(0.5f);
		lightDesc.direction = light->GetTransform()->GetRotation();
		light->GetLight()->SetLightDesc(lightDesc);
		CUR_SCENE->Add(light);
	}
	{
		
		// Sky
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"SkyBox");
		obj->GetOrAddTransform();
		obj->AddComponent(make_shared<SkyBox>());
		obj->GetSkyBox()->Init();		
		CUR_SCENE->Add(obj);
		
	}
	{

		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"Terrain");
		obj->GetOrAddTransform();
		obj->AddComponent(make_shared<Terrain>());

		auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");
		obj->GetTerrain()->Create(10,10 , mat);

		CUR_SCENE->Add(obj);

	}
	// Model
	{
		shared_ptr<class Model> m2 = make_shared<Model>();
		m2->ReadModel(L"Tower/Tower");
		m2->ReadMaterial(L"Tower/Tower");

		for (int i = 0; i < 20; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Tower" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.01f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			auto collider = make_shared<OBBBoxCollider>();
			collider->GetBoundingBox().Extents = Vec3(1.f);
			obj->AddComponent(collider);

			CUR_SCENE->Add(obj);
		}
	}
}

void EditorTool::Update()
{
	GET_SINGLE(ShortcutManager)->Update();
	GET_SINGLE(EditorToolManager)->Update();

	if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
	{
		int32 x = INPUT->GetMousePos().x;
		int32 y = INPUT->GetMousePos().y;
		shared_ptr<GameObject> obj = CUR_SCENE->Pick(x,y);

		if (obj != nullptr)
		{
			wstring name = obj->GetObjectName();
			int64 id = obj->GetId();
			TOOL->SetSelectedObjH(id);
			ADDLOG("Pick Object : " + Utils::ToString(name), LogFilter::Info);
		}
	
	}

	ImGui::ShowDemoWindow(&_showWindow);
}

void EditorTool::Render()
{
	
}

void EditorTool::OnMouseWheel(int32 scrollAmount)
{
	_sceneCam->MoveCam(scrollAmount);
}

void EditorTool::DrawGrid(float gridSize)
{
	
}
