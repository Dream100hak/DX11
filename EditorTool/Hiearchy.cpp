#include "pch.h"
#include "Hiearchy.h"
#include "UndoManager.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "LogWindow.h"
#include "ParticleSystem.h"
#include "SkyBox.h"

#include "Utils.h"
#include "Model.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Camera.h"

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

	// 오브젝트 선택 처리
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

	// 루트 오브젝트만 그리고 자식은 노드 안에서 재귀 (유니티식 트리)
	for (auto& object : gameObjects)
	{
		shared_ptr<GameObject> obj = object.second;
		if (obj == nullptr || obj->GetObjectName().empty())
			continue;

		auto tr = obj->GetTransform();
		if (tr != nullptr && tr->GetParent() != nullptr)
			continue; // 자식 — 부모 노드가 그림

		DrawHierarchyNode(obj);
	}

	// 빈 공간 드롭 = 루트로 (부모 해제)
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		avail.x = max(avail.x, 1.f);
		avail.y = max(avail.y, 40.f);
		ImGui::Dummy(avail);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HierarchyObj"))
			{
				int64 draggedId = *(const int64*)payload->Data;
				auto draggedObj = CUR_SCENE->GetCreatedObject((int32)draggedId);
				if (draggedObj != nullptr && draggedObj->GetTransform() != nullptr)
				{
					UndoManager::Record();
					draggedObj->GetTransform()->SetParentKeepWorld(nullptr);
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	if (ImGui::BeginPopupContextWindow())
	{
		int32 id = -1;

		if (ImGui::BeginMenu("GameObject"))
		{
			if (ImGui::MenuItem("Add Empty GameObject"))
			{
				UndoManager::Record(); id = GUI->CreateEmptyGameObject();
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Empty GameObject", LogFilter::Info);
			}

			if (ImGui::MenuItem("Create Cube"))
			{
				UndoManager::Record(); id = GUI->CreateMesh(CreatedObjType::CUBE);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Cube", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create Quad"))
			{
				UndoManager::Record(); id = GUI->CreateMesh(CreatedObjType::QUAD);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Quad", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create Sphere"))
			{
				UndoManager::Record(); id = GUI->CreateMesh(CreatedObjType::SPHERE);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Sphere", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create PBR Test Grid"))
			{
				CreatePbrTestGrid();
				ADDLOG("Create PBR Test Grid", LogFilter::Info);
			}
			if (ImGui::MenuItem("Create Camera"))
			{
				id = CreateGameCamera();
				ADDLOG("Create Game Camera", LogFilter::Info);
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
				UndoManager::Record(); id = GUI->CreateLight(0);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Directional Light", LogFilter::Info);
			}
			if (ImGui::MenuItem("Point Light"))
			{
				UndoManager::Record(); id = GUI->CreateLight(1);
				TOOL->SetSelectedObjH(id);
				ADDLOG("Create Point Light", LogFilter::Info);
			}
			if (ImGui::MenuItem("Spot Light"))
			{
				UndoManager::Record(); id = GUI->CreateLight(2);
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
				UndoManager::Record(); id = GUI->CreateEmptyGameObject();
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

// 트리 노드 1개 — 선택/더블클릭 포커스/드래그앤드롭 페어런팅 + 자식 재귀
void Hiearchy::DrawHierarchyNode(shared_ptr<GameObject> obj)
{
	auto tr = obj->GetTransform();
	const bool hasChildren = (tr != nullptr && tr->GetChildren().empty() == false);
	const bool selected = (SELECTED_H == obj->GetId());

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (hasChildren == false)
		flags |= ImGuiTreeNodeFlags_Leaf;
	if (selected)
		flags |= ImGuiTreeNodeFlags_Selected;

	string name = Utils::ToString(obj->GetObjectName());
	bool open = ImGui::TreeNodeEx((void*)(intptr_t)obj->GetId(), flags, "%s", name.c_str());

	// 클릭 = 선택 (화살표 토글 클릭은 제외)
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsItemToggledOpen() == false)
	{
		CUR_SCENE->UnPickAll();
		TOOL->SetSelectedObjH(obj->GetId());
		obj->SetUIPicked(true);
	}

	// 더블클릭 = 카메라 포커스 (Transform 없는 오브젝트는 스킵)
	if (selected && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		auto camera = CUR_SCENE->GetMainCamera();
		if (camera != nullptr && tr != nullptr)
		{
			auto camTr = camera->GetTransform();
			Vec3 look = camTr->GetLook();
			look.Normalize();
			camTr->SetPosition(tr->GetPosition() - look * 10.f);
		}
	}

	// 드래그 소스 — 페어런팅 페이로드 (id)
	if (tr != nullptr && ImGui::BeginDragDropSource())
	{
		int64 id = obj->GetId();
		ImGui::SetDragDropPayload("HierarchyObj", &id, sizeof(id));
		ImGui::Text("%s", name.c_str());
		ImGui::EndDragDropSource();
	}

	// 드롭 타겟 — 이 노드의 자식으로 (월드 유지, 순환은 SetParentKeepWorld 가 거부)
	if (tr != nullptr && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HierarchyObj"))
		{
			int64 draggedId = *(const int64*)payload->Data;
			auto draggedObj = CUR_SCENE->GetCreatedObject((int32)draggedId);
			if (draggedObj != nullptr && draggedObj != obj && draggedObj->GetTransform() != nullptr)
			{
				UndoManager::Record();
				draggedObj->GetTransform()->SetParentKeepWorld(tr);
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (open)
	{
		if (hasChildren)
		{
			// 드롭으로 children 이 변형될 수 있어 복사 후 순회
			vector<shared_ptr<Transform>> children = tr->GetChildren();
			for (auto& child : children)
			{
				if (auto childObj = child->GetGameObject())
					DrawHierarchyNode(childObj);
			}
		}
		ImGui::TreePop();
	}
}

int32 Hiearchy::CreateFire()
{
	UndoManager::Record();
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
	UndoManager::Record();
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
	UndoManager::Record();
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

// 게임 카메라 — 플레이 중 Game 뷰가 이 시점으로 렌더 (에디터 카메라와 별개)
int32 Hiearchy::CreateGameCamera()
{
	UndoManager::Record();
	int32 id = GUI->CreateEmptyGameObject();
	TOOL->SetSelectedObjH(id);

	auto obj = CUR_SCENE->GetCreatedObject(id);
	obj->GetTransform()->SetLocalScale(Vec3(1.f, 1.f, 1.f)); // CreateEmpty 의 0.01 스케일 원복

	// 고유 이름
	int32 cnt = 0;
	while (true)
	{
		wstring name = L"Game Camera_" + std::to_wstring(cnt);
		if (CUR_SCENE->FindCreatedObjectByName(name) == nullptr)
		{
			obj->SetObjectName(name);
			break;
		}
		cnt++;
	}

	obj->AddComponent(make_shared<Camera>());
	return id;
}

// PBR 寃利앹슜 援ъ껜 洹몃━????媛濡?roughness 0?? (6??, ?몃줈 metallic 0?? (4??
// 移대찓???꾨갑???ㅽ룿. ?뷀띁??Cook-Torrance ?쇱씠???뺤씤??
void Hiearchy::CreatePbrTestGrid()
{
	UndoManager::Record();
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
