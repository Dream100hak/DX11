#include "pch.h"
#include "SceneWindow.h"
#include "Camera.h"
#include "LogWindow.h"
#include "ShortcutManager.h"
#include "EditorToolManager.h"
#include "Utils.h"
#include "MathUtils.h"
#include "SkyBox.h"
#include "Light.h"
#include "SceneGrid.h"
#include "FolderContents.h"
#include "TerrainWindow.h"
#include "Terrain.h"
#include "RenderContext.h"  // 렌더 컨텍스트
#include "MeshRenderer.h"   // 메시 렌더러
#include "ModelRenderer.h"  // 모델 렌더러
#include "ModelAnimator.h"  // 모델 애니메이터

#include "Model.h"
#include "ImGuizmo.h"
#include "UndoManager.h"
#include <filesystem>




SceneWindow::SceneWindow(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos , size);
}

SceneWindow::~SceneWindow()
{

}

void SceneWindow::Init()
{
	// 씬 윈도우 렌더 타겟 생성 (초기 크기: 800x530)
	CreateRenderTarget(_sceneWidth, _sceneHeight);
}

void SceneWindow::CreateRenderTarget(uint32 width, uint32 height)
{
	_sceneWidth = width;
	_sceneHeight = height;

	// 1. Texture2D 생성 (RTV + SRV용)
	{
		D3D11_TEXTURE2D_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(texDesc));
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		HRESULT hr = DEVICE->CreateTexture2D(&texDesc, nullptr, _sceneTexture.GetAddressOf());
		CHECK(hr);
	}

	// 2. RenderTargetView 생성
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		ZeroMemory(&rtvDesc, sizeof(rtvDesc));
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		HRESULT hr = DEVICE->CreateRenderTargetView(_sceneTexture.Get(), &rtvDesc, _sceneRTV.GetAddressOf());
		CHECK(hr);
	}

	// 3. ShaderResourceView 생성 (ImGui 렌더링 표시용)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		HRESULT hr = DEVICE->CreateShaderResourceView(_sceneTexture.Get(), &srvDesc, _sceneSRV.GetAddressOf());
		CHECK(hr);
	}

	// 4. DepthStencil 생성
	{
		D3D11_TEXTURE2D_DESC depthDesc;
		ZeroMemory(&depthDesc, sizeof(depthDesc));
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.MiscFlags = 0;

		ComPtr<ID3D11Texture2D> depthTexture;
		HRESULT hr = DEVICE->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf());
		CHECK(hr);

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		ZeroMemory(&dsvDesc, sizeof(dsvDesc));
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		hr = DEVICE->CreateDepthStencilView(depthTexture.Get(), &dsvDesc, _sceneDSV.GetAddressOf());
		CHECK(hr);
	}

	// 5. Viewport 설정
	_sceneViewport.Set(width, height, 0, 0);
}

void SceneWindow::RenderScene()
{
	// 렌더 타겟 설정 후 씬 렌더링
	_sceneViewport.RSSetViewport();
	DCT->OMSetRenderTargets(1, _sceneRTV.GetAddressOf(), _sceneDSV.Get());

	// 백그라운드: Color 클리어 (초록색)
	Color clearColor(0.2f, 0.2f, 0.2f, 1.0f);
	DCT->ClearRenderTargetView(_sceneRTV.Get(), (float*)&clearColor);
	DCT->ClearDepthStencilView(_sceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 씬 정보 획득 (현재 씬)
	auto scene = SCENE->GetCurrentScene();
	if (!scene) return;

	auto mainCamera = scene->GetMainCamera();
	if (!mainCamera) return;

	auto camera = mainCamera->GetCamera();
	auto light = scene->GetLight();

	if (!camera) return;

	// 카메라 정렬 및 렌더링
	camera->SortGameObject();
	camera->Render_Forward();

	// SceneWindow 렌더 타겟 해제 후 메인 렌더 타겟 복원 (ImGui 렌더링을 위한 원래 RTV)
	GRAPHICS->RestoreMainRenderTarget();
}


void SceneWindow::Update()
{
	ShowSceneWindow();
}

void SceneWindow::Render()
{
	// 예약됨: SceneWindow 렌더링 함수 (향후 추가 렌더링 기능 구현)
}

void SceneWindow::ShowSceneWindow()
{

	const ImGuiIO& io = ImGui::GetIO();

	auto& prviewObjs =
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshPreviewObjs();

	auto& scales =
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshScales();

	// ── Scene 창 (도크 탭) — 씬은 RT 로 렌더되어 아래 Image 로 표시 ──
	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

	// 툴바(EditTransform 상단)와 기즈모는 이 창에 그려짐
	GAME->GetSceneDesc().drawList = ImGui::GetWindowDrawList();

	ImVec2 scenePos(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
	ImVec2 sceneSize(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height);

	// 패스 뷰어 콤보박스 (GBuffer 패스 선택, 씬 윈도우 좌상단)
	{
		ImGui::SetNextWindowPos(ImVec2(scenePos.x + 8.f, scenePos.y + 8.f));
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
		if (ImGui::Begin("PassViewerOverlay", nullptr, overlayFlags))
		{
			int mode = MAIN_CAM->GetDebugViewMode();
			ImGui::SetNextItemWidth(110.f);
			if (ImGui::Combo("##PassView", &mode,
				"Final\0Albedo\0Normal\0Roughness\0Metallic\0World Pos\0Depth\0SSAO\0Shadow\0"))
			{
				MAIN_CAM->SetDebugViewMode(mode);
			}
		}
		ImGui::End();
	}

	if (ImGui::BeginDragDropTargetCustom(ImRect(scenePos, scenePos + sceneSize), ImGui::GetID("Scene")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MeshPayload"))
		{
			MetaData** droppedMeshRawPtr = static_cast<MetaData**>(payload->Data);
			shared_ptr<MetaData> droppedMesh =	make_shared<MetaData>(**droppedMeshRawPtr);

			shared_ptr<GameObject> obj =  prviewObjs[droppedMesh->fileFullPath + L"/" + droppedMesh->fileName];
			CUR_SCENE->Remove(obj);

			UndoManager::Record(); // 모델 배치 직전 스냅샷

			int32 id = -1;
			if (droppedMesh->metaType == MetaType::CLIP)
			{
				// 클립 파일 로드 (파일명 = 모델명, 경로 = 모델경로 + 파일명으로 로드)
				wstring modelName = filesystem::path(droppedMesh->fileFullPath).filename().wstring();
				wstring modelPath = droppedMesh->fileFullPath + L'/' + modelName;
				auto model = RESOURCES->Get<Model>(modelPath);
				if (model)
				{
					int32 animIndex = model->GetAnimIndexByFileName(droppedMesh->fileName);
					id = GUI->CreateModelAnimatorMesh(model, obj->GetTransform()->GetPosition(), animIndex);
				}
			}
			else
			{
				// 메시 파일 로드 (일반 모델 배치)
				shared_ptr<Model> model = make_shared<Model>();
				wstring modelName = droppedMesh->fileName.substr(0, droppedMesh->fileName.find('.'));
				model->ReadModel(modelName + L'/' + modelName);

				// .mmat(바이너리) 우선, 없으면 레거시 .xml 폴백
				wstring mmatPath = L"../Resources/Assets/Models/" + modelName + L'/' + modelName + L".mmat";
				if (filesystem::exists(mmatPath))
					model->ReadMaterial(modelName + L'/' + modelName);
				else
					model->ReadMaterialByXml(modelName + L'/' + modelName);

				id = GUI->CreateModelMesh(model, obj->GetTransform()->GetPosition());
			}
			if (id != -1)
			{
				CUR_SCENE->UnPickAll();
				TOOL->SetSelectedObjH(id);
				CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);
			}

			ADDLOG("Create Object : " + Utils::ToString(droppedMesh->fileName) , LogFilter::Warn);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}
		ImGui::EndDragDropTarget();
	}


	EditTransform();

	if (_bUsing == false)
	{
		// PassViewer 콤보 등 씬뷰 위 ImGui 오버레이 클릭이 픽킹으로 흘러가지 않게 가드
		if (io.MouseClicked[0] && GUI->IsHoveringWindow())
		{
			int32 x = INPUT->GetMousePos().x;
			int32 y = INPUT->GetMousePos().y;

			if (GRAPHICS->IsMouseInViewport(x, y))
			{
				shared_ptr<GameObject> obj = CUR_SCENE->MeshPick(x, y);

				if (obj != nullptr)
				{
					if (obj->GetUIPickable())
					{
						CUR_SCENE->UnPickAll();
						obj->SetUIPicked(true);
					}

					wstring name = obj->GetObjectName();
					int64 id = obj->GetId();
					TOOL->SetSelectedObjH(id);

					ADDLOG("Pick Object : " + Utils::ToString(name), LogFilter::Info);
				}
				else
				{
					CUR_SCENE->UnPickAll();
					TOOL->SetSelectedObjH(-1);
				}
			}

		}
	}

	int64 id = TOOL->GetSelectedIdH();

	shared_ptr<GameObject> obj = SCENE->GetCurrentScene()->GetCreatedObject(id);
	_tr = id == -1 ? nullptr :  obj->GetTransform();

	ImGui::End(); // "Scene"
}

// 씬 RT 이미지 + SceneDesc(이미지 영역)/카메라 출력 갱신
// 씬 렌더는 다음 프레임 SCENE->Update 에서 이 RT 로 들어옴 (1프레임 지연 — 무해)
void SceneWindow::DrawSceneImage()
{
	ImVec2 avail = ImGui::GetContentRegionAvail();
	avail.x = max(avail.x, 64.f);
	avail.y = max(avail.y, 64.f);

	// 창 리사이즈 → RT 재생성
	uint32 w = (uint32)avail.x;
	uint32 h = (uint32)avail.y;
	if (w != _sceneWidth || h != _sceneHeight)
		CreateRenderTarget(w, h);

	// 에디터 카메라 디퍼드 출력을 이 RT 로 (매 프레임 보장 — 카메라/RT 재생성 대비)
	if (auto camObj = CUR_SCENE->GetMainCamera())
		camObj->GetCamera()->SetFinalOutput(_sceneRTV);

	ImVec2 imgPos = ImGui::GetCursorScreenPos();
	ImGui::Image(_sceneSRV.Get(), avail);

	// SceneDesc = 씬 이미지 영역 — 픽킹/기즈모/오버레이/뷰포트가 전부 이 rect 기준
	SceneDesc& sd = GAME->GetSceneDesc();
	sd.x = imgPos.x;
	sd.y = imgPos.y;
	sd.width = avail.x;
	sd.height = avail.y;
}

// ImGuizmo 기반 트랜스폼 기즈모 — 이동/회전/스케일 + Local/World + 스냅
// (기존 수제 기즈모는 회전 미구현이라 정식 ImGuizmo 로 교체)
void SceneWindow::EditTransform()
{
	static SceneWindow::Mode currentGizmoMode(SceneWindow::Local);
	static bool useSnap = false;
	static float snapTranslate = 0.5f;
	static float snapAngle = 15.f;   // 도 단위
	static float snapScale = 0.1f;

	// W/E/R 모드 단축키 — 우클릭 카메라 비행 중에는 무시 (WASD 충돌 방지)
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right) == false)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W)) _currentGizmoOperation = TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E)) _currentGizmoOperation = ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) _currentGizmoOperation = SCALE;
	}

	// ── 씬뷰 툴바 ──
	uint32 fps = GET_SINGLE(TimeManager)->GetFps();
	ImGui::Text("FPS : %d", fps);

	if (ImGui::RadioButton("Move(W)", _currentGizmoOperation == TRANSLATE)) _currentGizmoOperation = TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate(E)", _currentGizmoOperation == ROTATE)) _currentGizmoOperation = ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale(R)", _currentGizmoOperation == SCALE)) _currentGizmoOperation = SCALE;

	if (ImGui::RadioButton("Local", currentGizmoMode == Local)) currentGizmoMode = Local;
	ImGui::SameLine();
	if (ImGui::RadioButton("World", currentGizmoMode == World)) currentGizmoMode = World;
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &useSnap);

	// 씬 RT 이미지 — 기즈모는 같은 drawList 뒤에 그려져 이미지 위에 표시됨
	DrawSceneImage();

	// 라이트 기즈모 — 선택 여부와 무관하게 항상 표시 (아래 트랜스폼 기즈모 가드보다 먼저)
	DrawLightGizmos();

	// 터레인 편집 모드: 트랜스폼 기즈모/픽킹 대신 브러시 스컬프팅으로 전환
	if (TerrainWindow::S_Edit)
	{
		DrawTerrainBrush();
		_bUsing = true; // 씬뷰 픽킹 블록(_bUsing==false) 차단
		return;
	}

	if (_tr == nullptr || _tr->GetGameObject() == nullptr || _tr->GetGameObject()->IsIgnoredTransformEdit())
	{
		_bUsing = false;
		return;
	}

	// F: 선택 오브젝트로 카메라 포커스 (유니티 프레임 셀렉트)
	if (ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::IsMouseDown(ImGuiMouseButton_Right) == false)
	{
		auto camTr = CUR_SCENE->GetMainCamera()->GetTransform();
		Vec3 look = camTr->GetLook();
		look.Normalize();
		camTr->SetPosition(_tr->GetPosition() - look * 15.f);
	}

	// ── ImGuizmo ──
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(GAME->GetSceneDesc().drawList);

	Viewport& vp = GRAPHICS->GetViewport();
	ImGuizmo::SetRect(vp.GetPosX(), vp.GetPosY(), vp.GetWidth(), vp.GetHeight());

	Matrix view = MAIN_CAM->GetViewMatrix();
	Matrix proj = MAIN_CAM->GetProjectionMatrix();
	Matrix world = _tr->GetWorldMatrix();

	float snapValue = _currentGizmoOperation == ROTATE ? snapAngle
		: (_currentGizmoOperation == SCALE ? snapScale : snapTranslate);
	float snap[3] = { snapValue, snapValue, snapValue };

	// 전역 OPERATION 비트값은 ImGuizmo::OPERATION 과 동일 (수제 기즈모가 그대로 포팅했던 값)
	ImGuizmo::Manipulate(
		reinterpret_cast<float*>(&view), reinterpret_cast<float*>(&proj),
		static_cast<ImGuizmo::OPERATION>(_currentGizmoOperation),
		currentGizmoMode == Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
		reinterpret_cast<float*>(&world),
		nullptr, useSnap ? snap : nullptr);

	// 기즈모 드래그 시작 프레임 — 쓰기 전이므로 스냅샷은 이동 전 상태 (Undo 1회 = 드래그 1회 되돌림)
	static bool sGizmoWasUsing = false;
	const bool gizmoUsing = ImGuizmo::IsUsing();
	if (gizmoUsing && sGizmoWasUsing == false)
		UndoManager::Record();
	sGizmoWasUsing = gizmoUsing;

	if (ImGuizmo::IsUsing())
	{
		Vec3 scale, trans;
		Quaternion rot;

		// 특이 행렬(스케일 0 등)이면 Decompose 실패 — NaN 이 트랜스폼에 들어가면 복구 불가라 그 프레임은 버림
		if (world.Decompose(scale, rot, trans))
		{
			Vec3 euler = Transform::ToEulerAngles(rot);

			const float v[9] = { trans.x, trans.y, trans.z, euler.x, euler.y, euler.z, scale.x, scale.y, scale.z };
			bool finite = true;
			for (float f : v)
				finite &= (std::isfinite(f) != 0);

			if (finite)
			{
				// SetWorldMatrix: 부모가 있으면 월드→로컬 역변환 후 로컬 분해 — 계층에서도 정확
				// (기존 SetPosition/SetRotation/SetScale 분해는 부모 회전·스케일 합성이 깨짐)
				_tr->SetWorldMatrix(world);
				_tr->UpdateTransform();
			}
		}
	}

	// 기즈모 드래그/호버 중에는 씬뷰 픽킹 차단
	_bUsing = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
}

void SceneWindow::DrawLightGizmos()
{
	auto scene = CUR_SCENE;
	if (scene == nullptr || scene->GetMainCamera() == nullptr)
		return;

	const SceneDesc& sd = GAME->GetSceneDesc();
	ImDrawList* dl = sd.drawList;
	if (dl == nullptr)
		return;

	Matrix VP = MAIN_CAM->GetViewMatrix() * MAIN_CAM->GetProjectionMatrix();

	// 월드 → 씬 이미지 스크린 좌표 (카메라 뒤면 false)
	auto w2s = [&](const Vec3& w, ImVec2& out) -> bool
	{
		Vec4 c = Vec4::Transform(Vec4(w.x, w.y, w.z, 1.f), VP);
		if (c.w <= 0.001f)
			return false;
		float x = c.x / c.w, y = c.y / c.w;
		out = ImVec2(sd.x + (x * 0.5f + 0.5f) * sd.width, sd.y + (-y * 0.5f + 0.5f) * sd.height);
		return true;
	};
	auto line3d = [&](const Vec3& a, const Vec3& b, ImU32 col, float th)
	{
		ImVec2 pa, pb;
		if (w2s(a, pa) && w2s(b, pb))
			dl->AddLine(pa, pb, col, th);
	};
	// 라이트 방향 기준 정규직교 기저
	auto basis = [](Vec3 dir, Vec3& right, Vec3& up)
	{
		Vec3 wup = (fabsf(dir.y) > 0.99f) ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
		right = wup.Cross(dir); right.Normalize();
		up = dir.Cross(right);  up.Normalize();
	};

	for (auto& lo : scene->GetLights())
	{
		auto lc = lo->GetLight();
		if (lc == nullptr)
			continue;

		Vec3 pos = lo->GetTransform()->GetPosition();
		const bool selected = (SELECTED_H == lo->GetId());
		const ImU32 col = selected ? IM_COL32(255, 225, 70, 255) : IM_COL32(255, 215, 90, 170);

		// 라이트 위치 마커 (항상 표시)
		ImVec2 marker;
		if (w2s(pos, marker))
			dl->AddCircleFilled(marker, selected ? 5.f : 3.5f, col);

		Vec3 dir = lc->GetLightDesc().direction;
		if (dir.LengthSquared() < 1e-6f) dir = Vec3(0.f, -1.f, 0.f);
		dir.Normalize();
		Vec3 right, up; basis(dir, right, up);

		// 선택 안 된 라이트는 콘/구 와이어프레임 생략 — 화면이 지저분해지는 걸 막음 (유니티식).
		// 대신 어느 쪽을 비추는지 짧은 방향 선만 표시.
		if (selected == false)
		{
			if (lc->GetLightType() != Point)
				line3d(pos, pos + dir * 2.5f, col, 1.5f);
			continue;
		}

		if (lc->GetLightType() == Directional)
		{
			// 평행 광선 4개 + 중심 화살표
			const float r = 3.f, len = 14.f;
			for (int i = 0; i < 4; ++i)
			{
				float a = i * XM_PIDIV2;
				Vec3 off = right * (cosf(a) * r) + up * (sinf(a) * r);
				line3d(pos + off, pos + off + dir * len, col, 1.5f);
			}
			Vec3 tip = pos + dir * len;
			line3d(pos, tip, col, 2.f);
			// 화살촉
			line3d(tip, tip - dir * 3.f + right * 1.5f, col, 2.f);
			line3d(tip, tip - dir * 3.f - right * 1.5f, col, 2.f);
		}
		else if (lc->GetLightType() == Point)
		{
			const float rad = lc->GetRange();
			const int seg = 32;
			auto circle = [&](const Vec3& a1, const Vec3& a2)
			{
				ImVec2 prev; bool have = false;
				for (int i = 0; i <= seg; ++i)
				{
					float a = (float)i / seg * XM_2PI;
					Vec3 p = pos + a1 * (cosf(a) * rad) + a2 * (sinf(a) * rad);
					ImVec2 s;
					if (w2s(p, s)) { if (have) dl->AddLine(prev, s, col, 1.2f); prev = s; have = true; }
					else have = false;
				}
			};
			circle(Vec3(1, 0, 0), Vec3(0, 1, 0));
			circle(Vec3(1, 0, 0), Vec3(0, 0, 1));
			circle(Vec3(0, 1, 0), Vec3(0, 0, 1));
		}
		else // Spot
		{
			const float range = lc->GetRange();
			const float halfAng = lc->GetSpotAngleDeg() * XM_PI / 180.f;

			// 콘 끝을 range(공중)이 아니라 빛이 실제로 닿는 지면(y=0)까지 클램프 —
			// 공중에 뜬 원판이 "비추는 곳"과 달라 보이던 문제 해결. 아래로 향할 때만 적용.
			float coneLen = range;
			if (dir.y < -0.01f)
			{
				float t = -pos.y / dir.y; // pos + dir*t 의 y == 0
				if (t > 0.f && t < range)
					coneLen = t;
			}
			const float baseR = coneLen * tanf(halfAng);
			Vec3 baseC = pos + dir * coneLen;
			const int seg = 24;
			ImVec2 prev; bool have = false;
			for (int i = 0; i <= seg; ++i)
			{
				float a = (float)i / seg * XM_2PI;
				Vec3 p = baseC + right * (cosf(a) * baseR) + up * (sinf(a) * baseR);
				ImVec2 s;
				if (w2s(p, s)) { if (have) dl->AddLine(prev, s, col, 1.2f); prev = s; have = true; }
				else have = false;
			}
			for (int i = 0; i < 4; ++i) // apex → base 스포크
			{
				float a = i * XM_PIDIV2;
				Vec3 p = baseC + right * (cosf(a) * baseR) + up * (sinf(a) * baseR);
				line3d(pos, p, col, 1.4f);
			}
		}
	}
}

void SceneWindow::DrawTerrainBrush()
{
	auto scene = CUR_SCENE;
	shared_ptr<GameObject> terrainObj = scene ? scene->GetTerrain() : nullptr;
	shared_ptr<Terrain> terrain = terrainObj ? terrainObj->GetTerrain() : nullptr;
	if (terrain == nullptr)
		return;

	int32 mx = INPUT->GetMousePos().x;
	int32 my = INPUT->GetMousePos().y;
	if (GRAPHICS->IsMouseInViewport(mx, my) == false || GUI->IsHoveringWindow() == false)
		return;

	// 카메라 레이 ↔ 지형 교차로 브러시 중심 찾기
	Vec3 o, d;
	scene->ScreenToWorldRay(mx, my, o, d);
	Vec3 hit;
	if (terrain->RaycastTerrain(o, d, hit) == false)
		return;

	// 브러시 링 — 지형 표면을 따라 원을 샘플해 그림
	const SceneDesc& sd = GAME->GetSceneDesc();
	ImDrawList* dl = sd.drawList;
	if (dl != nullptr)
	{
		Matrix VP = MAIN_CAM->GetViewMatrix() * MAIN_CAM->GetProjectionMatrix();
		auto w2s = [&](const Vec3& w, ImVec2& out) -> bool
		{
			Vec4 c = Vec4::Transform(Vec4(w.x, w.y, w.z, 1.f), VP);
			if (c.w <= 0.001f) return false;
			out = ImVec2(sd.x + (c.x / c.w * 0.5f + 0.5f) * sd.width,
				sd.y + (-c.y / c.w * 0.5f + 0.5f) * sd.height);
			return true;
		};

		const int seg = 40;
		const float r = TerrainWindow::S_Radius;
		ImU32 col = (TerrainWindow::S_Mode == 1) ? IM_COL32(255, 120, 90, 230)  // Lower=빨강
			: IM_COL32(120, 220, 255, 230);                                    // 그 외=하늘
		ImVec2 prev; bool have = false;
		for (int i = 0; i <= seg; ++i)
		{
			float a = (float)i / seg * XM_2PI;
			float wx = hit.x + cosf(a) * r;
			float wz = hit.z + sinf(a) * r;
			float wy = terrain->GetHeight(wx, wz) + 0.1f;
			ImVec2 s;
			if (w2s(Vec3(wx, wy, wz), s)) { if (have) dl->AddLine(prev, s, col, 1.6f); prev = s; have = true; }
			else have = false;
		}
		// 중심 점
		ImVec2 cen;
		if (w2s(Vec3(hit.x, terrain->GetHeight(hit.x, hit.z) + 0.1f, hit.z), cen))
			dl->AddCircleFilled(cen, 3.f, col);
	}

	// 좌클릭 드래그 → 스컬프팅 (MouseDown = 누르고 있는 동안 매 프레임 적용)
	const ImGuiIO& io = ImGui::GetIO();
	if (io.MouseDown[0])
		terrain->Sculpt(hit, TerrainWindow::S_Radius, TerrainWindow::S_Strength, TerrainWindow::S_Mode);
}

