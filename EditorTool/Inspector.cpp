#include "pch.h"
#include "Inspector.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"

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
#include "Material.h"
#include "MeshThumbnail.h"

#include "FolderContents.h"
#include "Utils.h"

#include "SceneGrid.h"

Inspector::Inspector(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos , size);
}

Inspector::~Inspector()
{

}

void Inspector::Init()
{
	auto grid = RESOURCES->Load<Texture>(L"Grid", L"..\\Resources\\Assets\\Textures\\Grid.png");
	auto obj = RESOURCES->Load<Texture>(L"ObjIcon", L"..\\Resources\\Assets\\Textures\\Obj.png");

}

void Inspector::Update()
{
	ShowInspector();
}

void Inspector::ShowInspector()
{
	ImGui::SetNextWindowPos(GetEWinPos() , ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(GetEWinSize() , ImGuiCond_Appearing);

	ImGui::Begin("Inspector");

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

void Inspector::ShowInfoHiearchy()
{
	shared_ptr<GameObject> go = CUR_SCENE->GetCreatedObject(SELECTED_H);

	wstring objectName = go->GetObjectName();
	string objName = string(objectName.begin(), objectName.end());

	char modifiedName[256];
	strncpy_s(modifiedName, objName.c_str(), sizeof(modifiedName));

	auto icon = RESOURCES->Get<Texture>(L"ObjIcon")->GetComPtr().Get();
	ImGui::Image(icon, ImVec2(40, 40));
	ImGui::SameLine();

	ImGui::BeginGroup();
	static bool active = false;
	ImGui::Checkbox("##ActiveObj", &active);

	ImVec2 layerPos = ImGui::GetItemRectMin();

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f); // 남은 너비의 절반만큼
	ImGui::InputText(" ", modifiedName, sizeof(modifiedName));
	go->SetObjectName(wstring(modifiedName, modifiedName + strlen(modifiedName)));

	ImGui::SameLine(0.f , 1.f);

	static bool staticObj = false;
	ImGui::Checkbox("Static", &staticObj);

	//레이어 변경

	ImVec2 textPosition = ImVec2(layerPos.x , layerPos.y + 25.f );
	ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), "Layer");

	ImGui::Dummy(ImVec2(40,10));
	ImGui::SameLine();

	int selectedLayer = static_cast<int>(go->GetLayerIndex());
	if (ImGui::Combo("##Layer", &selectedLayer, "Default\0UI\0Wall\0Invisible\0"))
	{
		go->SetLayerIndex(selectedLayer);
	}

	ImGui::EndGroup();
	
	ImGui::Spacing();
	ImGui::Separator();


	for (int i = 0; i < (int)ComponentType::End - 1; i++)
	{
		ComponentType componentType = static_cast<ComponentType>(i);
		shared_ptr<Component> comp = go->GetFixedComponent(componentType);

		if (comp == nullptr)
			continue;

		string s = GUI->EnumToString(componentType);

		if (ImGui::TreeNodeEx(s.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			comp->OnInspectorGUI();
			ImGui::TreePop();
		}

		ImGui::Separator();

	}
	const auto& monoBehaviors = go->GetMonoBehaviours();
	for (auto& behaviors : monoBehaviors)
	{
		wstring rawName = behaviors->GetBehaviorName();
		string name = string(rawName.begin(), rawName.end());

		if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TreePop();
		}
	}

	if (ImGui::Button("Add Component", ImVec2(-1, 0)))
	{
		ImVec2 buttonPos = ImGui::GetCursorScreenPos();
		ImGui::OpenPopup("Add Component Menu");
		buttonPos.y += ImGui::GetTextLineHeightWithSpacing() * 0.4f;
		ImGui::SetNextWindowPos(buttonPos);
	}

	if (ImGui::BeginPopup("Add Component Menu"))
	{
		if (ImGui::BeginMenu("FixedComponent"))
		{
			for (int i = 0; i < (int)ComponentType::End - 1; i++)
			{
				ComponentType componentType = static_cast<ComponentType>(i);
				string fixedCompName = GUI->EnumToString(componentType);

				if (ImGui::MenuItem(fixedCompName.c_str()))
				{
					if (go->GetFixedComponent(componentType))
						continue;

					switch (componentType)
					{
					case ComponentType::Transform: go->AddComponent(make_shared<Transform>());
						break;
					case ComponentType::MeshRenderer: go->AddComponent(make_shared<MeshRenderer>());
						break;
						//	case ComponentType::ModelRenderer: go->AddComponent(make_shared<ModelRenderer>());
						//		break;
					case ComponentType::Camera: go->AddComponent(make_shared<Camera>());
						break;
						//	case ComponentType::Animator: go->AddComponent(make_shared<ModelAnimator>());
						//		break;
					case ComponentType::Light: go->AddComponent(make_shared<Light>());
						break;
					case ComponentType::Collider: go->AddComponent(make_shared<OBBBoxCollider>());
						break;
					case ComponentType::Terrain: go->AddComponent(make_shared<Terrain>());
						break;
					case ComponentType::Button: go->AddComponent(make_shared<Button>());
						break;
					case ComponentType::BillBoard: go->AddComponent(make_shared<Billboard>());
						break;
						//case ComponentType::SnowBillBoard: go->AddComponent(make_shared<SnowBillboard>());
						//	break;

					}
					//	go->AddComponent( GUI->CreateComponentByType(componentType) ) ;
						//shared_ptr<type_id(*this)> comp = make_shared<Component>(); // 이걸 어떻게 ComponentType에 맞게 바꾸냐고
					//	go->AddComponent("여기다가 componentType에 맞게 추가");
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("new Script"))
		{

		}

		ImGui::EndPopup();
	}
}

void Inspector::ShowInfoProject()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	string objName = string(metaData->fileName.begin(), metaData->fileName.end());
	if (metaData->metaType != MetaType::FOLDER)
		objName += " import setting";

	auto icon = GetMetaFileIcon();

	ImGui::Image(icon, ImVec2(50,50));
	ImGui::SameLine();

	ImVec2 buttonSize(40.f, 40.f);

	ImVec2 buttonPos = ImGui::GetItemRectMin();
	ImVec2 textPosition = ImVec2(buttonPos.x  + ( buttonSize.x  * 1.5f ) , buttonPos.y + (buttonSize.y * 0.5f));
	ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), objName.c_str());
	
	ImGui::Separator();
	ImGui::Spacing();


	// 이미지 파일 처리
	if (metaData->metaType == MetaType::IMAGE)
	{
		auto tex = RESOURCES->Get<Texture>(L"FILE_" + metaData->fileName);

		static int32 texType = 0;
		ImGui::Text("Texture Type ");
		ImGui::SameLine();
		ImGui::Combo("##Texture Type", &texType, "Default\0UI\0Wall\0Invisible\0");
		static int32 texShape = 0;
		ImGui::Text("Texture Shape");
		ImGui::SameLine();
		ImGui::Combo("##Texture Shape", &texShape, "Default\0UI\0Wall\0Invisible\0");

		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Texture Preview");
		ImGui::BeginGroup();
		
		ImGui::Button("RGB"); ImGui::SameLine();
		ImGui::Button("R"); ImGui::SameLine();
		ImGui::Button("G"); ImGui::SameLine();
		ImGui::Button("B");
		ImGui::EndGroup();
	
		float width =  min(tex->GetSize().x , ImGui::GetCurrentWindow()->Size.x);
		ImGui::Image(icon, ImVec2(width, width));
	
		string texInfo = "Size : %.0f X %.0f";
		char tmps[512];
		ImFormatString(tmps, sizeof(tmps), texInfo.c_str(), tex->GetSize().x , tex->GetSize().y);
		ImGui::Text(tmps);
	}
	// 매터리얼 파일 처리
	else if (metaData->metaType == MetaType::MATERIAL)
	{
		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Material Preview");

		ImGui::Image(icon, ImVec2(373, 400));

		auto folderContents = static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()));

		auto& previewsMeshObjs = folderContents->GetMeshPreviewObjs();
		auto& obj = previewsMeshObjs[metaData->fileFullPath + L'/' + metaData->fileName];

		auto& previewsThumbnails = folderContents->GetMeshPreviewThumbnails();
		auto& thumbnail = previewsThumbnails[metaData->fileFullPath + L'/' + metaData->fileName];

		auto cam = folderContents->GetCamera();
		auto light = folderContents->GetLight();

		shared_ptr<Material>& material = obj->GetMeshRenderer()->GetMaterial();
		MaterialDesc& desc = material->GetMaterialDesc();
		ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

		bool changed = false;

		if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) { changed = true; }
		if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) { changed = true; }
		if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) { changed = true; }
		if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) { changed = true; }

		if (changed)
		{
			JOB_POST_RENDER->DoPush([=]()
			{
				InstancingData data;
				data.world = obj->GetTransform()->GetWorldMatrix();
				data.isPicked = obj->GetUIPicked() ? 1 : 0;
				shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
				buffer->AddData(data);

				thumbnail->Draw(obj->GetMeshRenderer(), cam, light, buffer);
			});
		}
	}

	// 메시 파일 처리
	else if (metaData->metaType == MetaType::MESH)
	{
		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Mesh Preview");

		ImGui::Image(icon, ImVec2(373, 400));

	}

	ImGui::Separator();

}

ID3D11ShaderResourceView* Inspector::GetMetaFileIcon()
{	
	shared_ptr<MetaData> metaData = SELECTED_P;
	ID3D11ShaderResourceView* srv = nullptr;

	switch (metaData->metaType)
	{
		case FOLDER:
			srv = RESOURCES->Get<Texture>(L"Folder")->GetComPtr().Get();
			break;
		case META:
			break;
	
		case SOUND:
			break;
		case IMAGE:
			srv = RESOURCES->Load<Texture>(L"FILE_" + metaData->fileName, metaData->fileFullPath + L"\\" + metaData->fileName)->GetComPtr().Get();
			break;
		case MATERIAL:
		case MESH:		
			srv = GetMeshThumbnail()->GetComPtr().Get();
			break;
		case TEXT:
		case XML:
		case Unknown:
			srv = RESOURCES->Get<Texture>(L"Text")->GetComPtr().Get();
			break;
	
		default:
			break;
	}

	return srv;
}

class shared_ptr<MeshThumbnail>& Inspector::GetMeshThumbnail()
{

	shared_ptr<MetaData> metaData = SELECTED_P;

	auto& previewsThumbnails =
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshPreviewThumbnails();

	return previewsThumbnails[metaData->fileFullPath + L'/' + metaData->fileName];
	
}
