#include "pch.h"
#include "Hiearchy.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"
#include "ParticleSystem.h"
#include "SkyBox.h"

#include "Utils.h"
#include "Model.h"
#include "MeshRenderer.h"
#include "Material.h"

Hiearchy::Hiearchy(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
}

Hiearchy::~Hiearchy()
{

}

void Hiearchy::Init()
{

}

void Hiearchy::Update()
{
	ShowHiearchy();
}

void Hiearchy::ShowHiearchy()
{
	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());

	ImGui::Begin("Hiearchy", nullptr);

	// 占쏙옙占?占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
	if (ImGui::BeginDragDropTargetCustom(ImRect(GetEWinPos(), GetEWinPos() + GetEWinSize()), ImGui::GetID("Hiearchy")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MeshPayload"))
		{
			MetaData** metaPtr = static_cast<MetaData**>(payload->Data);
			shared_ptr<MetaData> metaData = std::make_shared<MetaData>(**metaPtr);

			if(metaData->metaType == MetaType::MESH)
			{
				auto model = RESOURCES->Get<Model>(metaData->fileFullPath + L"/" + metaData->fileName);

				int32 id = GUI->CreateModelMesh(model);
				CUR_SCENE->UnPickAll();
				TOOL->SetSelectedObjH(id);
				CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);
				
				ADDLOG("Create Model", LogFilter::Info);
			}

			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}
		ImGui::EndDragDropTarget();
	}


	ImGui::BeginChild("left pane", ImVec2(360, 0), true);

	const auto gameObjects = CUR_SCENE->GetCreatedObjects();
	ImGuiStyle& style = ImGui::GetStyle();

	for (auto& object : gameObjects)
	{
		wstring wstr = object.second->GetObjectName();
		if (wstr.empty())
			continue;
		string name = Utils::ToString(wstr);  // wstring占쏙옙string 占쏙옙환 占쏙옙占쏙옙

		bool isSelected = (SELECTED_H == object.first);

		if (isSelected)
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.58f, 1.0f, 1.f)); // Blue background
		else
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 0.2f)); // Default background

		if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))  // 占쏙옙표 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
		{
			CUR_SCENE->UnPickAll();
			TOOL->SetSelectedObjH(object.first);
			object.second->SetUIPicked(true);
		}

		if (isSelected && ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			auto camera = SCENE->GetCurrentScene()->GetMainCamera();
			if (camera)
			{
				Vec3 objPos = object.second->GetTransform()->GetPosition();
				auto cameraTransform = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();

				float distance = 10.0f; 

				Vec3 lookDirection = cameraTransform->GetLook();
				lookDirection.Normalize(); 

				Vec3 cameraPosition = objPos - (lookDirection * distance);

				cameraTransform->SetPosition(cameraPosition);
			}
		}

		ImGui::PopStyleColor();
	}

	if (ImGui::BeginPopupContextWindow())
	{
		int32 id = -1;

		if (ImGui::BeginMenu("GameObject"))
		{
			if (ImGui::MenuItem("Add Empty GameObject"))
			{
				id = GUI->CreateEmptyGameObject();
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Empty GameObject", LogFilter::Info);
			}

			if (ImGui::MenuItem("Create Cube"))
			{
				id = GUI->CreateMesh(CreatedObjType::CUBE);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Cube", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create Quad"))
			{
				id = GUI->CreateMesh(CreatedObjType::QUAD);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Quad", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create Sphere"))
			{
				id = GUI->CreateMesh(CreatedObjType::SPHERE);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Sphere", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create PBR Test Grid"))
			{
				CreatePbrTestGrid();
				ADDLOG("Create PBR Test Grid", LogFilter::Info);
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Particle"))
		{
			if (ImGui::MenuItem("Create Fire"))
			{
				id = CreateFire();
			}

			if (ImGui::MenuItem("Create Rain"))
			{
				id = CreateRain();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Light"))
		{
			if (ImGui::MenuItem("Directional Light"))
			{
				id = GUI->CreateLight(0);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Directional Light", LogFilter::Info);
			}
			if (ImGui::MenuItem("Point Light"))
			{
				id = GUI->CreateLight(1);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Point Light", LogFilter::Info);
			}
			if (ImGui::MenuItem("Spot Light"))
			{
				id = GUI->CreateLight(2);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Spot Light", LogFilter::Info);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Environment"))
		{	
			if (ImGui::MenuItem("Create Sky"))
			{
				id = CreateSky();
			}

			if (ImGui::MenuItem("Create Terrain"))
			{
				id = GUI->CreateEmptyGameObject();
				TOOL->SetSelectedObjH(id);

				ADDLOG("Create Terrain", LogFilter::Info);
			}
	
			ImGui::EndMenu();
		}

		if (id != -1)
		{
			CUR_SCENE->UnPickAll();
			CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();

	ImGui::End();
}

int32 Hiearchy::CreateFire()
{
	int32 id = GUI->CreateEmptyGameObject(CreatedObjType::PARTICLE);
	TOOL->SetSelectedObjH(id);

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetTransform();
	float distance = 30.0f;
	Vec3 pos = cam->GetLocalPosition() + (cam->GetLook() * distance);

	shared_ptr<GameObject> fire = CUR_SCENE->GetCreatedObject(id);
	fire->SetObjectName(L"Fire " + GUI->FindEmptyName(CreatedObjType::PARTICLE));
	//fire->GetOrAddTransform()->SetPosition(Vec3(107.f, 1.f, -28.f));
	fire->GetOrAddTransform()->SetPosition(pos);
	fire->AddComponent(make_shared<class ParticleSystem>());

	std::vector<wstring> names = { L"../Resources/Assets/Textures/flare0.dds" };

	fire->GetComponent<ParticleSystem>()->Init(PT_FIRE, names, 500);
	ADDLOG("Create Fire", LogFilter::Info);
	
	return id;
}

int32 Hiearchy::CreateRain()
{
	int32 id = GUI->CreateEmptyGameObject();
	TOOL->SetSelectedObjH(id);

	shared_ptr<GameObject> rainDrop = CUR_SCENE->GetCreatedObject(id);
	rainDrop->SetObjectName(L"Rain " + GUI->FindEmptyName(CreatedObjType::PARTICLE));
	rainDrop->GetOrAddTransform()->SetPosition(Vec3::Zero);
	rainDrop->AddComponent(make_shared<class ParticleSystem>());

	std::vector<wstring> names = { L"../Resources/Assets/Textures/raindrop.dds" };
	rainDrop->GetComponent<ParticleSystem>()->Init(PT_RAIN, names, 10000);

	ADDLOG("Create Rain", LogFilter::Info);

	return id;
}

int32 Hiearchy::CreateSky()
{
	int32 id = GUI->CreateEmptyGameObject();
	TOOL->SetSelectedObjH(id);

	auto obj = CUR_SCENE->GetCreatedObject(id);
	obj->SetObjectName(L"SkyBox");
	obj->AddComponent(make_shared<SkyBox>());
	obj->GetComponent<SkyBox>()->Init();

	ADDLOG("Create Sky", LogFilter::Info);

	return id;
}

int32 Hiearchy::CreateTerrain()
{
	return 0;
}

// PBR 寃利앹슜 援ъ껜 洹몃━????媛濡?roughness 0?? (6??, ?몃줈 metallic 0?? (4??
// 移대찓???꾨갑???ㅽ룿. ?뷀띁??Cook-Torrance ?쇱씠???뺤씤??
void Hiearchy::CreatePbrTestGrid()
{
	auto scene = SCENE->GetCurrentScene();
	auto cam = scene->GetMainCamera();
	if (cam == nullptr)
		return;

	auto sphereMesh = RESOURCES->Get<Mesh>(L"Sphere");
	auto baseMat = RESOURCES->Get<Material>(L"DefaultMaterial");
	if (sphereMesh == nullptr || baseMat == nullptr)
		return;

	Vec3 base = cam->GetTransform()->GetLocalPosition() + cam->GetTransform()->GetLook() * 25.f;

	static int32 gridCount = 0;
	gridCount++;

	for (int32 my = 0; my < 4; ++my)
	{
		for (int32 rx = 0; rx < 6; ++rx)
		{
			auto obj = make_shared<GameObject>();
			obj->SetObjectName(L"PbrTest" + to_wstring(gridCount) + L"_m" + to_wstring(my) + L"_r" + to_wstring(rx));
			obj->GetOrAddTransform()->SetPosition(base + Vec3(rx * 2.4f, my * 2.4f, 0.f));
			obj->GetOrAddTransform()->SetScale(Vec3(1.f, 1.f, 1.f));

			auto pbrMat = baseMat->Clone();
			pbrMat->SetDiffuseMap(nullptr); // ?쒖닔 而щ윭濡?
			pbrMat->GetMaterialDesc().diffuse = Color(1.0f, 0.3f, 0.25f, 1.f);
			pbrMat->GetMaterialDesc().roughness = 0.05f + rx * 0.19f;
			pbrMat->GetMaterialDesc().metallic = my / 3.f;
			pbrMat->SetRenderQueue(RenderQueue::Opaque);

			auto mr = make_shared<MeshRenderer>();
			mr->SetMesh(sphereMesh);
			mr->SetMaterial(pbrMat);
			obj->AddComponent(mr);
			CUR_SCENE->Add(obj);
		}
	}
}
