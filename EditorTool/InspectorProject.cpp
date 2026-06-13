#include "pch.h"
#include "Inspector.h"
#include "EditorToolManager.h"

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"

#include "Material.h"
#include "MeshThumbnail.h"

#include "SimpleGrid.h"

#include "FolderContents.h"
#include "LogWindow.h"
#include "Utils.h"

#include "ModelAnimation.h"
#include "ModelMesh.h"
#include "Transform.h"

// -----------------------------------------------------------
// Inspector — 프로젝트 모드 (선택된 파일의 타입별 임포트 설정/프리뷰)
// IMAGE / MATERIAL / MESH / CLIP
// -----------------------------------------------------------

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
		ShowProjectImage(metaData, icon);
	}
	// 매터리얼 파일 처리
	else if (metaData->metaType == MetaType::MATERIAL)
	{
		ShowProjectMaterial(metaData, icon);
	}
	// 메시 파일 처리
	else if (metaData->metaType == MetaType::MESH)
	{
		ShowProjectMesh(metaData);
	}
	// 클립 파일 처리
	else if (metaData->metaType == MetaType::CLIP)
	{
		ShowProjectClip(metaData);
	}

	ImGui::Separator();

}

void Inspector::ShowProjectImage(shared_ptr<MetaData>& metaData, ID3D11ShaderResourceView* icon)
{
	auto tex = RESOURCES->Get<Texture>(L"FILE_" + metaData->fileName);

	// Texture Type/Shape 임포트 설정과 RGB 채널 보기는 아직 미구현 — 죽은 UI 제거
	ImGui::Dummy(ImVec2(0, 50.f));
	ImGui::SeparatorText("Texture Preview");

	if (tex == nullptr) // 파일이 사라졌거나 아직 로드 전 — 크래시 방지
	{
		ImGui::TextDisabled("미리보기를 불러올 수 없습니다");
		return;
	}

	float width = min(tex->GetSize().x, ImGui::GetCurrentWindow()->Size.x);
	ImGui::Image(icon, ImVec2(width, width));

	string texInfo = "Size : %.0f X %.0f";
	char tmps[512];
	ImFormatString(tmps, sizeof(tmps), texInfo.c_str(), tex->GetSize().x, tex->GetSize().y);
	ImGui::Text(tmps);
}

void Inspector::ShowProjectMaterial(shared_ptr<MetaData>& metaData, ID3D11ShaderResourceView* icon)
{
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 50.f));
	ImGui::SeparatorText("Material Preview");
	ImGui::Image(icon, ImVec2(300, 300));
	ImGui::Separator();

	auto folderContents = static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()));

	// find() 가드 — operator[] 는 키 부재 시 null 을 삽입해 아래 역참조에서 크래시
	// (썸네일 캐시 상한 도입으로 항목이 제거될 수 있음 — 다음 프레임에 lazy 재생성됨)
	const wstring previewKey = metaData->fileFullPath + L'/' + metaData->fileName;
	auto& previewsMeshObjs = folderContents->GetMeshPreviewObjs();
	auto& previewsThumbnails = folderContents->GetMeshPreviewThumbnails();

	auto objIt = previewsMeshObjs.find(previewKey);
	auto thumbIt = previewsThumbnails.find(previewKey);
	shared_ptr<GameObject> obj = (objIt != previewsMeshObjs.end()) ? objIt->second : nullptr;
	shared_ptr<MeshThumbnail> thumbnail = (thumbIt != previewsThumbnails.end()) ? thumbIt->second : nullptr;

	if (obj == nullptr || obj->GetMeshRenderer() == nullptr || obj->GetMeshRenderer()->GetMaterial() == nullptr || thumbnail == nullptr)
	{
		ImGui::TextDisabled("Preview not loaded yet");
		return; // Begin/End 는 호출자(ShowInspector)가 관리
	}

	auto light = folderContents->GetLight();

	shared_ptr<Material>& material = obj->GetMeshRenderer()->GetMaterial();
	MaterialDesc& desc = material->GetMaterialDesc();
	ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

	std::string shaderName = "(no shader)";
	if (auto hlsl = material->GetHlslShader())
		shaderName = Utils::ToString(hlsl->GetName());

	ImGui::Text(shaderName.c_str());

	bool changed = false;

	if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) { changed = true; }
	if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) { changed = true; }
	if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) { changed = true; }
	if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) { changed = true; }
	ImGui::TextDisabled("Ambient/Specular: Forward(Transparent) only - Deferred uses Diffuse+PBR+Emissive");

	ImGui::SeparatorText("PBR");
	if (ImGui::SliderFloat("Roughness", &desc.roughness, 0.f, 1.f)) { changed = true; }
	if (ImGui::SliderFloat("Metallic", &desc.metallic, 0.f, 1.f)) { changed = true; }

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

		// AABB 자동 핏 — 폴더 썸네일과 동일 프레이밍으로 갱신
		Matrix V, P;
		MeshThumbnail::ComputeFitViewProj(obj, 1.f, V, P);

		// 즉시 렌더 (구 잡큐는 ImGui 이후 실행 — 1프레임 지연)
		thumbnail->Draw(renderers, V, P, light, buffers);
	}

	// 편집 영속화 — 같은 인스턴스를 쓰는 씬 모델에는 즉시 반영되지만, 파일 저장은 명시적으로
	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::Button("Save Material", ImVec2(-FLT_MIN, 0)))
	{
		wstring stem = metaData->fileName.substr(0, metaData->fileName.find('.'));
		material->Save(metaData->fileFullPath + L'/' + stem);
		ADDLOG("Save Material : " + Utils::ToString(metaData->fileName), LogFilter::Info);
	}
}

void Inspector::ShowProjectMesh(shared_ptr<MetaData>& metaData)
{
	//TODO : DRAW Inspector Mesh
	if (_previewObjName != metaData->fileName)
	{
		CreateMeshPreviewObj();
		DrawInspectorMesh();
		_previewObjName = metaData->fileName;
		_selectedBone = nullptr; // 모델 변경 — 이전 모델 본 선택 해제
	}

	shared_ptr<Model> model = _meshPreviewObj->GetModelRenderer()->GetModel();

	// Model/Animation/Material 탭 버튼과 Extract Textures/Materials 버튼은 미구현 — 죽은 UI 제거

	ImGui::Dummy(ImVec2(0, 50.f));
	ImGui::SeparatorText("Mesh Preview");
	ImVec2 imagePos = ImGui::GetCursorScreenPos();
	ImVec2 imageSize(373, 373);
	ImGui::Image(_meshthumbnail->GetComPtr().Get(), imageSize);

	if (model->GetBoneCount() > 0)
	{
		ImGui::Checkbox("Show Skeleton", &_showSkeleton);
		if (_showSkeleton)
			DrawSkeletonOverlay(model, imagePos, imageSize);
	}

	ImGui::Dummy(ImVec2(0, 10.f));

	DrawModelDetails(model);
}

// 프리뷰 위 본 오버레이 — 바인드포즈 관절 라인/점, 선택 본 강조 (유니티 아바타 뷰 참조)
void Inspector::DrawSkeletonOverlay(shared_ptr<Model> model, const ImVec2& imagePos, const ImVec2& imageSize)
{
	if (_meshPreviewObj == nullptr)
		return;

	// 썸네일과 동일 프레이밍 — ComputeFitViewProj 는 결정적이라 재계산 = 드로우 때 V/P
	Matrix V, P;
	MeshThumbnail::ComputeFitViewProj(_meshPreviewObj, 1.f, V, P);
	Matrix WVP = _meshPreviewObj->GetTransform()->GetWorldMatrix() * V * P;

	// 본 transform 은 모델 공간 글로벌 (PushModelBoneData 와 동일 해석) — translation = 관절 위치
	auto project = [&](const Vec3& modelPos, ImVec2& out) -> bool
	{
		Vec4 clip = Vec4::Transform(Vec4(modelPos.x, modelPos.y, modelPos.z, 1.f), WVP);
		if (clip.w <= 0.f)
			return false;
		out.x = imagePos.x + (clip.x / clip.w * 0.5f + 0.5f) * imageSize.x;
		out.y = imagePos.y + (-clip.y / clip.w * 0.5f + 0.5f) * imageSize.y;
		return true;
	};

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 lineCol  = IM_COL32(80, 220, 120, 200);
	const ImU32 jointCol = IM_COL32(255, 255, 255, 220);
	const ImU32 selCol   = IM_COL32(255, 160, 40, 255);

	for (auto& bone : model->GetBones())
	{
		ImVec2 p;
		if (project(bone->transform.Translation(), p) == false)
			continue;

		if (bone->parent)
		{
			ImVec2 pp;
			if (project(bone->parent->transform.Translation(), pp))
				dl->AddLine(pp, p, lineCol, 1.5f);
		}

		const bool selected = (bone == _selectedBone);
		dl->AddCircleFilled(p, selected ? 4.5f : 2.5f, selected ? selCol : jointCol);
	}
}

// 모델 상세 — 요약/메시 테이블/스켈레톤(관절) 트리/머티리얼/클립 (유니티 모델 임포터 인스펙터 참조)
void Inspector::DrawModelDetails(shared_ptr<Model> model)
{
	if (model == nullptr)
		return;

	uint32 totalVtx = 0, totalIdx = 0;
	for (auto& mesh : model->GetMeshes())
	{
		totalVtx += mesh->geometry->GetVertexCount();
		totalIdx += mesh->geometry->GetIndexCount();
	}

	ImGui::SeparatorText("Model Info");
	ImGui::Text("Meshes : %u   Vertices : %u   Triangles : %u", model->GetMeshCount(), totalVtx, totalIdx / 3);
	ImGui::Text("Bones  : %u   Clips : %u   Materials : %u", model->GetBoneCount(), model->GetAnimationCount(), model->GetMaterialCount());
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("##MeshTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Verts");
			ImGui::TableSetupColumn("Tris");
			ImGui::TableSetupColumn("Material");
			ImGui::TableHeadersRow();

			for (auto& mesh : model->GetMeshes())
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", Utils::ToString(mesh->name).c_str());
				ImGui::TableNextColumn(); ImGui::Text("%u", mesh->geometry->GetVertexCount());
				ImGui::TableNextColumn(); ImGui::Text("%u", mesh->geometry->GetIndexCount() / 3);
				ImGui::TableNextColumn(); ImGui::Text("%s", Utils::ToString(mesh->materialName).c_str());
			}
			ImGui::EndTable();
		}
	}

	// 스켈레톤 — 본 부모/자식 계층을 그대로 트리로 (children 은 BindCacheInfo 가 채움)
	if (model->GetBoneCount() > 0 && ImGui::CollapsingHeader("Skeleton"))
	{
		std::function<void(const shared_ptr<ModelBone>&)> drawBone = [&](const shared_ptr<ModelBone>& bone)
		{
			if (bone == nullptr)
				return;

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (bone->children.empty())
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (bone == _selectedBone)
				flags |= ImGuiTreeNodeFlags_Selected;

			string label = Utils::ToString(bone->name) + "##bone" + std::to_string(bone->index);
			bool open = ImGui::TreeNodeEx(label.c_str(), flags);
			if (ImGui::IsItemClicked() && ImGui::IsItemToggledOpen() == false)
				_selectedBone = bone;
			ImGui::SameLine();
			ImGui::TextDisabled("[%d]", bone->index);

			if (open && bone->children.empty() == false)
			{
				for (auto& child : bone->children)
					drawBone(child);
				ImGui::TreePop();
			}
		};

		for (auto& bone : model->GetBones())
		{
			if (bone->parentIndex < 0)
				drawBone(bone);
		}

		// 선택 본 정보 — 바인드포즈(모델 공간) 트랜스폼
		if (_selectedBone != nullptr)
		{
			ImGui::Spacing();
			ImGui::SeparatorText(Utils::ToString(_selectedBone->name).c_str());
			ImGui::Text("Index : %d   Parent : %s   Children : %d",
				_selectedBone->index,
				_selectedBone->parent ? Utils::ToString(_selectedBone->parent->name).c_str() : "(none)",
				(int32)_selectedBone->children.size());

			Vec3 s, t;
			Quaternion r;
			if (_selectedBone->transform.Decompose(s, r, t))
			{
				Vec3 euler = Transform::ToEulerAngles(r) * (180.f / XM_PI);
				ImGui::Text("Position : %.3f, %.3f, %.3f", t.x, t.y, t.z);
				ImGui::Text("Rotation : %.1f, %.1f, %.1f", euler.x, euler.y, euler.z);
				ImGui::Text("Scale    : %.3f, %.3f, %.3f", s.x, s.y, s.z);
			}
			else
			{
				ImGui::TextDisabled("(non-decomposable transform)");
			}
		}
	}

	if (model->GetMaterialCount() > 0 && ImGui::CollapsingHeader("Materials"))
	{
		for (auto& mat : model->GetMaterials())
			ImGui::BulletText("%s", Utils::ToString(mat->GetName()).c_str());
	}

	if (model->GetAnimationCount() > 0 && ImGui::CollapsingHeader("Clips"))
	{
		for (auto& ani : model->GetAnimations())
			ImGui::BulletText("%s  (%u frames, %.0f fps, %.2fs)",
				Utils::ToString(ani->fileName).c_str(), ani->frameCount, ani->frameRate,
				ani->frameRate > 0.f ? ani->frameCount / ani->frameRate : 0.f);
	}
}

void Inspector::ShowProjectClip(shared_ptr<MetaData>& metaData)
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
	ImGui::Text("%u frames | %.0f fps | %.2fs",
		animation->frameCount, animation->frameRate,
		animation->frameRate > 0.f ? animation->frameCount / animation->frameRate : 0.f);

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
}

void Inspector::CreateMeshPreviewObj()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	wstring modelName = metaData->fileName.substr(0, metaData->fileName.find('.'));
	wstring modelPath = metaData->fileFullPath + L'/' + modelName;

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

	_meshPreviewObj->AddComponent(make_shared<ModelRenderer>());
	_meshPreviewObj->GetModelRenderer()->SetModel(model);
}


void Inspector::CreateAniPreviewObj()
{
	_isPlayingAnim = false;
	_animationProgress = 0.0f;

	shared_ptr<MetaData> metaData = SELECTED_P;

	auto path = filesystem::path(metaData->fileFullPath);
	wstring modelName = path.filename().wstring();
	wstring modelPath = metaData->fileFullPath + L'/' + modelName;

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

	_meshPreviewObj->AddComponent(make_shared<ModelAnimator>());
	_meshPreviewObj->GetModelAnimator()->SetModel(model);

	// 프리뷰 V/P 는 ComputeFitViewProj 로 직접 계산 — 카메라를 obj 에 부모 연결하던 구 방식 제거
}

void Inspector::DrawInspectorMesh()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	_meshthumbnail = make_shared<MeshThumbnail>(512, 512);

	std::vector<shared_ptr<Renderer>> renderers;
	std::vector<shared_ptr<InstancingBuffer>> buffers;

	// 바닥 그리드는 MeshThumbnail 이 내부에서 깐다 (구형 _simpleGrid 제거 — 화면 밖 + 페이드 없음)
	renderers.push_back(_meshPreviewObj->GetRenderer());

	for (int32 i = 0; i < renderers.size(); i++)
	{
		InstancingData data;
		data.world = renderers[i]->GetTransform()->GetWorldMatrix();
		data.isPicked = renderers[i]->GetGameObject()->GetUIPicked() ? 1 : 0;
		shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
		buffer->AddData(data);
		buffers.push_back(buffer);
	}

	// AABB 자동 핏 — 정사각 RT(512²) 기준 중앙 정렬
	Matrix V, P;
	MeshThumbnail::ComputeFitViewProj(_meshPreviewObj, 1.f, V, P);

	// 즉시 렌더 (구 잡큐는 ImGui 이후 실행 — 1프레임 지연)
	_meshthumbnail->Draw(renderers, V, P, _meshPreviewLight->GetLight(), buffers);
}

void Inspector::DrawInspectorClip()
{
	shared_ptr<MetaData> metaData = SELECTED_P;

	std::vector<shared_ptr<Renderer>> renderers;
	std::vector<shared_ptr<InstancingBuffer>> buffers;

	// 바닥 그리드는 MeshThumbnail 이 내부에서 깐다 (구형 _simpleGrid 제거)
	renderers.push_back(_meshPreviewObj->GetRenderer());

	for (int32 i = 0; i < renderers.size(); i++)
	{
		InstancingData data;
		data.world = renderers[i]->GetTransform()->GetWorldMatrix();
		data.isPicked = renderers[i]->GetGameObject()->GetUIPicked() ? 1 : 0;
		shared_ptr<InstancingBuffer> buffer = make_shared<InstancingBuffer>();
		buffer->AddData(data);
		buffers.push_back(buffer);
	}

	// AABB 자동 핏 — 정사각 RT(512²) 기준 중앙 정렬
	Matrix V, P;
	MeshThumbnail::ComputeFitViewProj(_meshPreviewObj, 1.f, V, P);

	// 즉시 렌더 (구 잡큐는 ImGui 이후 실행 — 1프레임 지연)
	_meshthumbnail->Draw(renderers, V, P, _meshPreviewLight->GetLight(), buffers);
}

void Inspector::PickMaterialTexture(string textureType, OUT bool& changed)
{
	shared_ptr<MetaData> metaData = SELECTED_P;
	if (metaData == nullptr)
		return;

	auto folderContents = static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()));

	// find() 가드 — operator[] 의 null 삽입/역참조 크래시 방지
	const wstring previewKey = metaData->fileFullPath + L'/' + metaData->fileName;
	auto& previewsMeshObjs = folderContents->GetMeshPreviewObjs();
	auto& previewsThumbnails = folderContents->GetMeshPreviewThumbnails();

	auto objIt = previewsMeshObjs.find(previewKey);
	auto thumbIt = previewsThumbnails.find(previewKey);
	shared_ptr<GameObject> obj = (objIt != previewsMeshObjs.end()) ? objIt->second : nullptr;
	shared_ptr<MeshThumbnail> thumbnail = (thumbIt != previewsThumbnails.end()) ? thumbIt->second : nullptr;

	if (obj == nullptr || obj->GetMeshRenderer() == nullptr || obj->GetMeshRenderer()->GetMaterial() == nullptr || thumbnail == nullptr)
		return;

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
	{
		// 파일이 사라졌으면(이름변경/삭제된 항목을 아직 선택 중) GetOrAddTexture 가 null 반환 — 역참조 금지
		auto tex = RESOURCES->GetOrAddTexture(L"FILE_" + metaData->fileName, metaData->fileFullPath + L"\\" + metaData->fileName);
		srv = (tex != nullptr) ? tex->GetComPtr().Get() : nullptr;
		break;
	}
	case MATERIAL:
	case MESH:
	case CLIP:
		if (auto& thumb = GetMeshThumbnail())
			srv = thumb->GetComPtr().Get();
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
	// find() 가드 — operator[] 는 키 부재 시 null 삽입 (캐시 제거와 맞물리면 크래시)
	static shared_ptr<MeshThumbnail> sNull = nullptr;

	shared_ptr<MetaData> metaData = SELECTED_P;
	if (metaData == nullptr)
		return sNull;

	auto& previewsThumbnails =
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshPreviewThumbnails();

	auto it = previewsThumbnails.find(metaData->fileFullPath + L'/' + metaData->fileName);
	return (it != previewsThumbnails.end()) ? it->second : sNull;
}
