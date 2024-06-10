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

#include "Button.h"
#include "OBBBoxCollider.h"
#include "SkyBox.h"
#include "Material.h"
#include "MeshThumbnail.h"

#include "SimpleGrid.h"

#include "FolderContents.h"
#include "Utils.h"

#include "SceneGrid.h"
#include "SkyCubeMap.h"
#include "ModelAnimation.h"

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
	ImGui::SetNextWindowPos(GetEWinPos(), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(GetEWinSize(), ImGuiCond_Appearing);

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

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
	ImGui::InputText(" ", modifiedName, sizeof(modifiedName));
	go->SetObjectName(wstring(modifiedName, modifiedName + strlen(modifiedName)));

	ImGui::SameLine(0.f, 1.f);

	static bool staticObj = false;
	ImGui::Checkbox("Static", &staticObj);

	///////////////////////////////////////////////////
	//					 LAYER                       //
	///////////////////////////////////////////////////

	ImVec2 textPosition = ImVec2(layerPos.x, layerPos.y + 25.f);
	ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), "Layer");

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::SameLine();

	int selectedLayer = static_cast<int>(go->GetLayerIndex());
	if (ImGui::Combo("##Layer", &selectedLayer, "Default\0UI\0Wall\0Invisible\0"))
	{
		go->SetLayerIndex(selectedLayer);
	}

	ImGui::EndGroup();

	ImGui::Spacing();
	ImGui::Separator();

	///////////////////////////////////////////////////
	//               COMPONENT                      //
	///////////////////////////////////////////////////

	for (int i = 0; i < (int)ComponentType::End - 1; i++)
	{
		ComponentType componentType = static_cast<ComponentType>(i);
		shared_ptr<Component> comp = go->GetFixedComponent(componentType);

		if (comp == nullptr)
			continue;

		string name = GUI->EnumToString(componentType);
		ImGui::PushID(comp.get());
		ShowComponentInfo(comp, name);
	}

	///////////////////////////////////////////////////
	//               MONOBEHAVIOUR                   //
	///////////////////////////////////////////////////

	const auto& monoBehaviors = go->GetMonoBehaviours();
	for (auto& behaviors : monoBehaviors)
	{
		wstring rawName = behaviors->GetBehaviorName();
		string name = string(rawName.begin(), rawName.end());
		ImGui::PushID(behaviors.get());
		ShowComponentInfo(behaviors, name);
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
			std::vector<ComponentType> compTypes = 
			{
				ComponentType::Camera,
				ComponentType::Light,
				ComponentType::Collider,
				ComponentType::Button,
				ComponentType::BillBoard,
			};

			std::vector<string> renderTypes = 
			{
				Utils::GetClassNameEX<MeshRenderer>(),
				Utils::GetClassNameEX<ModelRenderer>(),
				Utils::GetClassNameEX<ModelAnimator>(),
			};

			for (int32 i = 0 ; i < renderTypes.size(); i++)
			{
				if (go->GetFixedComponent(ComponentType::Renderer))
					continue;

				if (ImGui::MenuItem(renderTypes[i].c_str()))
				{
					/*		switch (i)
							{
							case 0:
								go->AddComponent(make_shared<MeshRenderer>());

							case 1:
								go->AddComponent(make_shared<ModelRenderer>());

							case 2:
								go->AddComponent(make_shared<ModelAnimator>());
							}*/
				}
			}

			for (auto componentType : compTypes)
			{
				string fixedCompName = GUI->EnumToString(componentType);

				if (go->GetFixedComponent(componentType))
					continue;

				if (ImGui::MenuItem(fixedCompName.c_str()))
				{
					switch (componentType)
					{
					case ComponentType::Camera: go->AddComponent(make_shared<Camera>());
						break;
					case ComponentType::Light: go->AddComponent(make_shared<Light>());
						break;
					case ComponentType::Collider: go->AddComponent(make_shared<OBBBoxCollider>());
						break;
					case ComponentType::Button: go->AddComponent(make_shared<Button>());
						break;
					case ComponentType::BillBoard: go->AddComponent(make_shared<Billboard>());
						break;
					}
				}
			}

			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
}

void Inspector::ShowComponentInfo(shared_ptr<Component> component, string name)
{
	shared_ptr<GameObject> go = CUR_SCENE->GetCreatedObject(SELECTED_H);

	bool open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);

	float spacing = ImGui::GetStyle().ItemInnerSpacing.x + 5.f;
	ImGui::SameLine(ImGui::GetWindowWidth() - spacing - ImGui::CalcTextSize("Delete").x - ImGui::GetScrollX() - spacing);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // 빨간색
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // 마우스 오버 시 색상
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f)); // 버튼 클릭 시 색상

	if (ImGui::Button("Delete"))
	{
		ImGui::OpenPopup("Confirm Delete");
	}

	ImGui::PopStyleColor(3);

	if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to delete this component?");
		ImGui::Separator();
		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			//go->RemoveComponent(comp);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (open)
	{
		component->OnInspectorGUI();

		ImGui::TreePop();
	}

	ImGui::PopID();
	ImGui::Separator();

}

void Inspector::ShowMeshModelInfo()
{

}

void Inspector::ShowMeshAnimationInfo()
{

}

void Inspector::ShowMeshMaterialInfo()
{

}

void Inspector::ShowInfoProject()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	string objName = string(metaData->fileName.begin(), metaData->fileName.end());
	if (metaData->metaType != MetaType::FOLDER)
		objName += " import setting";

	auto icon = GetMetaFileIcon();

	ImGui::Image(icon, ImVec2(50, 50));
	ImGui::SameLine();

	ImVec2 buttonSize(40.f, 40.f);

	ImVec2 buttonPos = ImGui::GetItemRectMin();
	ImVec2 textPosition = ImVec2(buttonPos.x + (buttonSize.x * 1.5f), buttonPos.y + (buttonSize.y * 0.5f));
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

		float width = min(tex->GetSize().x, ImGui::GetCurrentWindow()->Size.x);
		ImGui::Image(icon, ImVec2(width, width));

		string texInfo = "Size : %.0f X %.0f";
		char tmps[512];
		ImFormatString(tmps, sizeof(tmps), texInfo.c_str(), tex->GetSize().x, tex->GetSize().y);
		ImGui::Text(tmps);
	}
	// 매터리얼 파일 처리
	else if (metaData->metaType == MetaType::MATERIAL)
	{
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Material Preview");
		ImGui::Image(icon, ImVec2(300, 300));
		ImGui::Separator();

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

		shared_ptr<Shader> shader = material->GetShader();
		std::string shaderName = Utils::ToString(shader->GetName());

		ImGui::Text(shaderName.c_str());

		bool changed = false;

		if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) { changed = true; }
		if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) { changed = true; }
		if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) { changed = true; }
		if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) { changed = true; }

		if (ImGui::InputInt("Light Count", &desc.lightCount)) { changed = true; }
		if (ImGui::Checkbox("Use Texture", (bool*)&desc.useTexture)) { changed = true; }
		if (ImGui::Checkbox("Use Alpha Clip", (bool*)&desc.useAlphaclip)) { changed = true; }
		if (ImGui::Checkbox("Use SSAO", (bool*)&desc.useSsao)) { changed = true; }


		PickMaterialTexture("Diffuse", changed);
		ImGui::SameLine(0.f, -2.f);
		PickMaterialTexture("Normal", changed);
		ImGui::SameLine();
		PickMaterialTexture("Specular", changed);
		ImGui::SameLine();

		if (changed)
		{
			std::vector<shared_ptr<Renderer>> renderers;
			std::vector<shared_ptr<InstancingBuffer>> buffers;

			renderers.push_back(obj->GetMeshRenderer());

			InstancingData data;
			data.world = obj->GetTransform()->GetWorldMatrix();
			shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
			buffer->AddData(data);
			buffers.push_back(buffer);

			Matrix V = cam->GetViewMatrix();
			Matrix P = cam->GetProjectionMatrix();

			JOB_POST_RENDER->DoPush([=]()
			{
				thumbnail->Draw(renderers, V, P, light, buffers);
			});
		}
	}

	// 메시 파일 처리
	else if (metaData->metaType == MetaType::MESH)
	{
		//TODO : DRAW Inspector Mesh 
		if (_previewObjName != metaData->fileName)
		{
			CreateMeshPreviewObj();
			DrawInspectorMesh();
			_previewObjName = metaData->fileName;
		}

		shared_ptr<Model> model = _meshPreviewObj->GetModelRenderer()->GetModel();

		ImGui::Dummy(ImVec2(0, 20.f));

		// Add buttons at the top
		if (ImGui::Button("Model")) {}
		ImGui::SameLine();
		if (ImGui::Button("Animation")) {}
		ImGui::SameLine();
		if (ImGui::Button("Material")) {}


		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Mesh Preview");
		ImGui::Image(_meshthumbnail->GetComPtr().Get(), ImVec2(373, 373));

		ImGui::Dummy(ImVec2(0, 30.f));

		// Textures section
		ImGui::Text("Textures");
		ImGui::SameLine();
		float spaceTextures = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Extract Textures...").x - ImGui::GetStyle().ItemSpacing.x;
		ImGui::Dummy(ImVec2(spaceTextures - 15, 0)); // Fill the space
		ImGui::SameLine();
		if (ImGui::Button("Extract Textures..."))
		{
			// Handle Extract Textures button click
		}

		// Materials section
		ImGui::Text("Materials");
		ImGui::SameLine();
		float spaceMaterials = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Extract Materials...").x - ImGui::GetStyle().ItemSpacing.x;
		ImGui::Dummy(ImVec2(spaceMaterials - 10, 0)); // Fill the space
		ImGui::SameLine();
		if (ImGui::Button("Extract Materials..."))
		{
			// Handle Extract Materials button click
		}

		auto& materials = model->GetMaterials();
		auto& animations = model->GetAnimations();

		for (auto& mat : materials)
		{
			string matName = Utils::ToString(mat->GetName());
			ImGui::Text(matName.c_str());
			ImGui::SameLine();
			float space = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(matName.c_str()).x - ImGui::GetStyle().ItemSpacing.x;
			ImGui::Dummy(ImVec2(space - 20, 0)); // Fill the space
			ImGui::SameLine();

			// Push style color and variable to customize button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));  // Gray color
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Slightly lighter gray when hovered
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));  // Slightly darker gray when clicked
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);  // Rounded corners
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));  // Padding inside button

			ImGui::Button(Utils::ToString(mat->GetName()).c_str());

			// Pop style color and variable to revert to default
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

		ImGui::Dummy(ImVec2(0, 20.f));

		for (auto& ani : animations)
		{
			string aniName = Utils::ToString(ani->fileName).c_str();
			ImGui::Text(aniName.c_str());
			ImGui::SameLine();
			float space = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(aniName.c_str()).x - ImGui::GetStyle().ItemSpacing.x;
			ImGui::Dummy(ImVec2(space - 20, 0)); // Fill the space
			ImGui::SameLine();

			// Push style color and variable to customize button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));  // Gray color
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Slightly lighter gray when hovered
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));  // Slightly darker gray when clicked
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);  // Rounded corners
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));  // Padding inside button

			ImGui::Button(Utils::ToString(ani->fileName).c_str());

			// Pop style color and variable to revert to default
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

	}
	// 클립 파일 처리
	else if (metaData->metaType == MetaType::CLIP)
	{
		//TODO : DRAW Inspector Mesh 
		if (_previewObjName != metaData->fileName)
		{
			CreateAniPreviewObj();
			_previewObjName = metaData->fileName;
			_meshthumbnail = make_shared<MeshThumbnail>(512, 512);
		}

		DrawInspectorClip();

		shared_ptr<ModelAnimator> animator = _meshPreviewObj->GetModelAnimator();
		TweenDesc& desc = animator->GetTweenDesc();
		shared_ptr<ModelAnimation> animation = animator->GetModel()->GetAnimationByFileName(metaData->fileName);

		desc.curr.animIndex = animator->GetModel()->GetAnimIndexByFileName(metaData->fileName);

		std::string frameStr = "Frame : " + Utils::ToString((int32)desc.curr.currFrame);

		ImGui::Dummy(ImVec2(0, 50.f));
		ImGui::SeparatorText("Clip Preview");

		ImVec2 imagePos = ImGui::GetCursorScreenPos();
		ImVec2 imageSize(373, 373);

		ImGui::Image(_meshthumbnail->GetComPtr().Get(), imageSize);

		// Draw text inside the image
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(imagePos.x + imageSize.x / 2.5f, imagePos.y + imageSize.y - 20.0f),
			IM_COL32(255, 255, 255, 255),
			frameStr.c_str(),
			(const char*)0);

		ImVec4 originalFrameBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
		ImVec4 originalSliderGrab = ImGui::GetStyle().Colors[ImGuiCol_SliderGrab];
		ImVec4 originalSliderGrabActive = ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive];


		bool progressChanged = false;

		// Animation Control Buttons and Progress Bar on the same line
		if (ImGui::Button(_isPlayingAnim ? "Pause" : "Play "))
		{
			_isPlayingAnim = !_isPlayingAnim;
		}

		ImGui::SameLine();
		float availableWidth = ImGui::GetContentRegionAvail().x;
		float progressBarWidth = availableWidth * 0.6f;
		float speedSliderWidth = availableWidth * 0.3f;

		ImGui::PushItemWidth(progressBarWidth);
		ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 1);  // Black background
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = ImVec4(1, 1, 1, 1);  // White bar
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = ImVec4(1, 1, 1, 1);  // White bar active

		if (ImGui::SliderInt("##AnimationFrame", (int*)&desc.curr.currFrame, 0, animation->frameCount - 1 , ""))
		{
			progressChanged = true;

			desc.curr.currFrame = desc.curr.currFrame + 1;
			desc.curr.nextFrame = desc.curr.currFrame + 1;
		}

		ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = originalFrameBg;
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = originalSliderGrab;
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = originalSliderGrabActive;
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth(speedSliderWidth);
		ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 1);  // Black background
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f);  // Round slider grab
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = ImVec4(1, 1, 1, 1);  // White grab color
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = ImVec4(1, 1, 1, 1);  // White grab color active

		ImGui::SliderFloat("##AnimationSpeed", &desc.curr.speed, 1.f, 5.0f, "");
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 q = ImGui::GetItemRectMax();
		float lineY = (p.y + q.y) / 2;  // Midpoint of the slider
		float lineStartX = p.x;  // Start at the beginning of the slider
		float lineEndX = q.x;  // End at the end of the slider
		ImGui::GetWindowDrawList()->AddLine(ImVec2(lineStartX, lineY), ImVec2(lineEndX, lineY), IM_COL32(128, 128, 128, 255), 1.0f);
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = originalSliderGrab;
		ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = originalSliderGrabActive;
		ImGui::PopStyleVar(1);
		ImGui::PopItemWidth();

		// Move speed text to the right end
		char speedText[16];
		snprintf(speedText, sizeof(speedText), "%.1fx", desc.curr.speed);
		float textWidth = ImGui::CalcTextSize(speedText).x;

		ImGui::SameLine(0, availableWidth - progressBarWidth - speedSliderWidth - textWidth);
		ImGui::Text(speedText);  // Display the speed multiplier

		//static bool followCamera = false;
		//
		//if (ImGui::Checkbox("Follow Camera", (bool*)followCamera)) {}
		

		if(_isPlayingAnim)
			progressChanged = true;

		if (progressChanged)
		{
			_meshPreviewObj->GetModelAnimator()->UpdateTweenData();
		}

		_meshPreviewCamera->GetCamera()->UpdateMatrix();
	}

	ImGui::Separator();

}

void Inspector::CreateMeshPreviewObj()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	wstring modelName = metaData->fileName.substr(0, metaData->fileName.find('.'));
	wstring modelPath = metaData->fileFullPath + L'/' + modelName;

	auto shader = RESOURCES->Get<Shader>(L"Thumbnail");
	auto model = RESOURCES->Get<Model>(modelPath);

	BoundingBox box = model->CalculateModelBoundingBox();
	_meshPreviewObj = make_shared<GameObject>();

	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = globalScale / modelScale;
//	scale = 0.01f;

	_meshPreviewObj->GetOrAddTransform()->SetPosition(Vec3::Zero);
	_meshPreviewObj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	_meshPreviewObj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	_meshPreviewObj->AddComponent(make_shared<ModelRenderer>(shader));
	_meshPreviewObj->GetModelRenderer()->SetModel(model);
	_meshPreviewObj->GetModelRenderer()->SetPass(1);
}


void Inspector::CreateAniPreviewObj()
{
	_isPlayingAnim = false;
	_animationProgress = 0.0f;

	shared_ptr<MetaData> metaData = SELECTED_P;

	auto path = filesystem::path(metaData->fileFullPath);
	wstring modelName = path.filename().wstring();
	wstring modelPath = metaData->fileFullPath + L'/' + modelName;

	auto shader = RESOURCES->Get<Shader>(L"Standard");
	auto model = RESOURCES->Get<Model>(modelPath);

	BoundingBox box = model->CalculateModelBoundingBox();
	_meshPreviewObj = make_shared<GameObject>();

	float modelScale = max(max(box.Extents.x, box.Extents.y), box.Extents.z) * 2.0f;
	float globalScale = MODEL_GLOBAL_SCALE;

	if (modelScale > 10.f)
		modelScale = globalScale;

	float scale = globalScale / modelScale;
	//scale = 1.f;

	_meshPreviewObj->GetOrAddTransform()->SetPosition(Vec3::Zero);
	_meshPreviewObj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));
	_meshPreviewObj->GetOrAddTransform()->SetScale(Vec3(scale, scale, scale));

	_meshPreviewObj->AddComponent(make_shared<ModelAnimator>(shader));
	_meshPreviewObj->GetModelAnimator()->SetModel(model);
	_meshPreviewObj->GetModelAnimator()->SetPass(2);

	_meshPreviewCamera->GetTransform()->SetParent(_meshPreviewObj->GetTransform());
}

void Inspector::DrawInspectorMesh()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	_meshthumbnail = make_shared<MeshThumbnail>(512, 512);

	std::vector<shared_ptr<Renderer>> renderers;
	std::vector<shared_ptr<InstancingBuffer>> buffers;

	renderers.push_back(_meshPreviewObj->GetRenderer());
	renderers.push_back(_simpleGrid->GetRenderer());
//	renderers.push_back(_sceneGrid->GetRenderer());
	//for (auto& grid : _sceneGrids)
	//	renderers.push_back(grid->GetRenderer());

	for (int32 i = 0; i < renderers.size(); i++)
	{
		InstancingData data;
		data.world = renderers[i]->GetTransform()->GetWorldMatrix();
		data.isPicked = renderers[i]->GetGameObject()->GetUIPicked() ? 1 : 0;
		shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
		buffer->AddData(data);
		buffers.push_back(buffer);
	}

	//SKy는 그냥 인스턴싱 없이 렌더링
 // renderers.push_back(_skyBox->GetRenderer());
//	shared_ptr<InstancingBuffer> buffer = nullptr;
//	buffers.push_back(buffer);

	Matrix V = _meshPreviewCamera->GetCamera()->GetViewMatrix();
	Matrix P = _meshPreviewCamera->GetCamera()->GetProjectionMatrix();

	JOB_POST_RENDER->DoPush([=]()
	{
		_meshthumbnail->Draw(renderers, V, P, _meshPreviewLight->GetLight(), buffers);
	});
}

void Inspector::DrawInspectorClip()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	std::vector<shared_ptr<Renderer>> renderers;
	std::vector<shared_ptr<InstancingBuffer>> buffers;

	renderers.push_back(_meshPreviewObj->GetRenderer());
	renderers.push_back(_simpleGrid->GetRenderer());
	//renderers.push_back(_sceneGrid->GetRenderer());

	//for(auto& grid : _sceneGrids)
	//	renderers.push_back(grid->GetRenderer());

	for (int32 i = 0; i < renderers.size(); i++)
	{
		InstancingData data;
		data.world = renderers[i]->GetTransform()->GetWorldMatrix();
		data.isPicked = renderers[i]->GetGameObject()->GetUIPicked() ? 1 : 0;
		shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
		buffer->AddData(data);
		buffers.push_back(buffer);
	}

	Matrix V = _meshPreviewCamera->GetCamera()->GetViewMatrix();
	Matrix P = _meshPreviewCamera->GetCamera()->GetProjectionMatrix();

	JOB_POST_RENDER->DoPush([=]()
	{
		_meshthumbnail->Draw(renderers, V, P, _meshPreviewLight->GetLight(), buffers);
	});
}

void Inspector::PickMaterialTexture(string textureType, OUT bool& changed)
{
	shared_ptr<MetaData> metaData = SELECTED_P;

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

	ImGui::BeginGroup();
	ImGui::TextColored(color, textureType.c_str());

	ID3D11ShaderResourceView* srv = nullptr;

	if (textureType == "Diffuse")
		srv = material->GetDiffuseMap() != nullptr ? material->GetDiffuseMap()->GetComPtr().Get() : RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get();
	else if (textureType == "Normal")
		srv = material->GetNormalMap() != nullptr ? material->GetNormalMap()->GetComPtr().Get() : RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get();
	else if (textureType == "Specular")
		srv = material->GetSpecularMap() != nullptr ? material->GetSpecularMap()->GetComPtr().Get() : RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get();

	string popupName = "Select " + textureType + " Texture";

	if (ImGui::ImageButton(srv, ImVec2(75, 75)))
	{
		ImGui::OpenPopup(popupName.c_str());
	}

	if (ImGui::BeginPopup(popupName.c_str()))
	{
		for (auto& textureFile : CASHE_FILE_LIST)
		{
			if (textureFile.second->metaType != MetaType::IMAGE)
				continue;

			// 텍스처 미리보기를 위한 SRV 로드
			auto previewTexture = RESOURCES->GetOrAddTexture(L"FILE_" + textureFile.second->fileName, textureFile.second->fileFullPath + L"\\" + textureFile.second->fileName);
			ID3D11ShaderResourceView* previewSRV = previewTexture->GetComPtr().Get();

			if (ImGui::ImageButton(previewSRV, ImVec2(50, 50)))
			{
				if (textureType == "Diffuse")
					material->SetDiffuseMap(previewTexture);
				else if (textureType == "Normal")
					material->SetNormalMap(previewTexture);
				else if (textureType == "Specular")
					material->SetSpecularMap(previewTexture);

				obj->GetMeshRenderer()->SetTechnique(0);
				changed = true;

			}

			// 텍스처 파일 이름 표시
			ImGui::Text("%s", Utils::ToString(textureFile.second->fileName).c_str());
		}

		ImGui::EndPopup();
	}

	ImGui::EndGroup();
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
		srv = RESOURCES->GetOrAddTexture(L"FILE_" + metaData->fileName, metaData->fileFullPath + L"\\" + metaData->fileName)->GetComPtr().Get();
		break;
	case MATERIAL:
		srv = GetMeshThumbnail()->GetComPtr().Get();
		break;
	case MESH:
		srv = GetMeshThumbnail()->GetComPtr().Get();
		break;
	case CLIP:
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


