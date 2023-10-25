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

#include "MainMenuBar.h"
#include "GameEditorWindow.h"
#include "Hiearchy.h"
#include "Inspector.h"
#include "Project.h"

#include "Material.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include <boost/describe.hpp>
#include <boost/mp11.hpp>



void EditorTool::Init()
{
	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	auto menuBar = make_shared<MainMenuBar>();
	auto gameWnd = make_shared<GameEditorWindow>();
	auto hiearchy = make_shared<Hiearchy>();
	auto inspector = make_shared<Inspector>();
	auto project = make_shared<Project>();

	_editorWindows.push_back(menuBar);
	_editorWindows.push_back(gameWnd);
	_editorWindows.push_back(hiearchy);
	_editorWindows.push_back(inspector);
	_editorWindows.push_back(project);

	for (auto wnd : _editorWindows)
	{
		if(wnd == nullptr)
			continue;

		wnd->Init();
	}

	auto shader = RESOURCES->Get<Shader>(L"Standard");

	// Camera
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Scene Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->AddComponent(make_shared<SceneCamera>());
		CUR_SCENE->Add(camera);
	}
	
	// Light
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
		
		// Object
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(L"SkyBox");
		obj->GetOrAddTransform();
		obj->AddComponent(make_shared<SkyBox>());
		obj->GetSkyBox()->Init();
		
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
			CUR_SCENE->Add(obj);
		}
	}
}

void EditorTool::Update()
{
	GET_SINGLE(ShortcutManager)->Update();
	GET_SINGLE(EditorToolManager)->Update();

	ImGui::ShowDemoWindow(&_showWindow);

	for (auto wnd : _editorWindows)
	{
		if (wnd == nullptr)
			continue;

		wnd->Update();
	}
}

void EditorTool::Render()
{

}



