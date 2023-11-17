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

	//converter->ReadAssetFile(L"Hyejin/Hyejin_S002_LOD1.fbx");
	//converter->ExportMaterialData(L"Hyejin/Hyejin");
//	converter->ExportModelData(L"Hyejin/Hyejin");

	converter->ReadAssetFile(L"Juno/NPC_Inn_Vari01_Mesh.fbx");
	converter->ReadAssetFile(L"Juno/NPC_Inn_Facial_Mesh.fbx");
	converter->ExportMaterialData(L"Juno/Juno");
	converter->ExportModelData(L"Juno/Juno");

	GET_SINGLE(ShortcutManager)->Init();
	GET_SINGLE(EditorToolManager)->Init();

	_sceneCam = make_shared<SceneCamera>();


	auto shader = RESOURCES->Get<Shader>(L"Standard");

	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetObjectName(L"Scene Camera");
		camera->GetOrAddTransform()->SetPosition(Vec3{ -15.f, 14.f, -5.f });
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

		lightDesc.ambient = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
		lightDesc.diffuse = Vec4(0.7f, 0.7f, 0.6f, 1.0f);
		lightDesc.specular = Vec4(0.8f, 0.8f, 0.7f, 1.0f);
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


	// Model
	{

		shared_ptr<class Model> m2 = make_shared<Model>();

		m2->ReadModel(L"Hyejin/Hyejin");
		m2->ReadMaterial(L"Hyejin/Hyejin");

		for (int i = 0; i < 10; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Model_" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.1f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
			
			auto collider = make_shared<OBBBoxCollider>();
			collider->GetBoundingBox().Extents = Vec3(1.f);
			obj->AddComponent(collider);

			CUR_SCENE->Add(obj);
		}
	}
	{

		shared_ptr<class Model> m2 = make_shared<Model>();
			m2->ReadModel(L"Kachujin/Kachujin");
			m2->ReadMaterial(L"Kachujin/Kachujin");

		for (int i = 10; i < 20; i++)
		{
			auto obj = make_shared<GameObject>();
			wstring name = L"Model_" + to_wstring(i);
			obj->SetObjectName(name);

			obj->GetOrAddTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
			obj->GetOrAddTransform()->SetScale(Vec3(0.1f));

			obj->AddComponent(make_shared<ModelRenderer>(shader));
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);

			auto collider = make_shared<OBBBoxCollider>();
			collider->GetBoundingBox().Extents = Vec3(1.f);
			obj->AddComponent(collider);

			CUR_SCENE->Add(obj);
		}
	}
	// Model
	{
		shared_ptr<class Model> m2 = make_shared<Model>();

		m2->ReadModel(L"Tower/Tower");
		m2->ReadMaterial(L"Tower/Tower");
		auto obj = make_shared<GameObject>();
		wstring name = L"Tower";
		obj->SetObjectName(name);

		obj->GetOrAddTransform()->SetPosition(Vec3(30.f,0.f,70.f));
		obj->GetOrAddTransform()->SetScale(Vec3(0.06f));

		obj->AddComponent(make_shared<ModelRenderer>(shader));
		obj->GetModelRenderer()->SetModel(m2);
		obj->GetModelRenderer()->SetPass(1);

		auto collider = make_shared<OBBBoxCollider>();
		collider->GetBoundingBox().Extents = Vec3(1.f);
		obj->AddComponent(collider);

		CUR_SCENE->Add(obj);
	}

	{
		shared_ptr<class Model> m2 = make_shared<Model>();

		m2->ReadModel(L"Juno/Juno");
		m2->ReadMaterial(L"Juno/Juno");
		auto obj = make_shared<GameObject>();
		wstring name = L"Juno";
		obj->SetObjectName(name);

		obj->GetOrAddTransform()->SetPosition(Vec3(30.f, 0.f, 70.f));
		obj->GetOrAddTransform()->SetScale(Vec3(0.06f));

		obj->AddComponent(make_shared<ModelRenderer>(shader));
		obj->GetModelRenderer()->SetModel(m2);
		obj->GetModelRenderer()->SetPass(1);

		auto collider = make_shared<OBBBoxCollider>();
		collider->GetBoundingBox().Extents = Vec3(1.f);
		obj->AddComponent(collider);

		CUR_SCENE->Add(obj);
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

		if (GRAPHICS->IsMouseInViewport(x, y))
		{
			shared_ptr<GameObject> obj = CUR_SCENE->Pick(x, y);

			if (obj != nullptr)
			{
				wstring name = obj->GetObjectName();
				int64 id = obj->GetId();
				TOOL->SetSelectedObjH(id);
				ADDLOG("Pick Object : " + Utils::ToString(name), LogFilter::Info);

				//ADDLOG("Shadow : "  , LogFilter::Info);
			}
		}
	}

	ImGui::ShowDemoWindow(&_showWindow);
	Manipulate(OPERATION::TRANSLATE);


	auto shadowMap = GRAPHICS->GetShadowMap();
	auto shadowTex = static_pointer_cast<Texture>(shadowMap);

	ImGui::Begin("DirectX11 Texture Test");
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


 bool EditorTool::IsTranslateType(int type)
{
	return type >= MT_MOVE_X && type <= MT_MOVE_SCREEN;
}
 float EditorTool::IntersectRayPlane(const Vec4& rOrigin, const Vec4& rVector, const Vec4& plan)
 {
	 const float numer = plan.Dot(rOrigin) - plan.w;
	 const float denom = plan.Dot(rVector);

	 if (fabsf(denom) < FLT_EPSILON)  // normal is orthogonal to vector, cant intersect
	 {
		 return -1.0f;
	 }

	 return -(numer / denom);
 }
bool EditorTool::HandleTranslation(OPERATION op, int& type, const float* snap)
{
	if (!Intersects(op, TRANSLATE) || type != MT_NONE)
		return false;

}
bool EditorTool::Manipulate(OPERATION operation)
{
	shared_ptr<GameObject> selectedGo = CUR_SCENE->GetCreatedObject(TOOL->GetSelectedIdH());

	if (selectedGo == nullptr)
		return false;

	shared_ptr<Transform> tr = selectedGo->GetTransform();
	ComputeContext(tr);

	int type = MT_NONE;
	bool manipulated = false;

	//DrawRotationGizmo(operation);
	DrawTranslationGizmo(type, operation);
	//DrawScaleGizmo(operation);

	return manipulated;
}

void EditorTool::ComputeContext(shared_ptr<Transform> obj)
{
	if(obj == nullptr)
		return;

	GAME->GetSceneDesc().bMouseOver = GUI->IsHoveringWindow();
	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();

	_model = obj->GetWorldMatrix();
	_modelLocal = obj->GetLocalMatrix();
	_view = cam->GetViewMatrix();
	_projection = cam->GetProjectionMatrix();
	_modelInverse = _model.Invert();
	_modelSource = obj->GetWorldMatrix();
	_modelSourceInverse = _modelSource.Invert();

	_vp = _view  * _projection;
	_mvp = obj->GetWorldMatrix() * _view * _projection;
	_mvpLocal = obj->GetLocalMatrix() * _view, _projection;

	Matrix viewInverse = XMMatrixInverse(nullptr, _vp);
	XMVECTOR cameraRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

	cameraRight = XMVector3TransformNormal(cameraRight, XMMatrixTranspose(viewInverse));
	cameraRight = XMVector3TransformCoord(cameraRight, _projection);

	XMVECTOR cameraRightScreenSpace = XMVector3TransformCoord(cameraRight, _view * _projection);
	float rightLength = XMVectorGetX(XMVector3Length(cameraRightScreenSpace));

	GAME->GetSceneDesc().screenFactor = GAME->GetSceneDesc().gizmoSizeClipSpace / rightLength;

	ImVec2 centerSpace = GUI->WorldToScreenPos(Vec3::Zero, _mvp);
	GAME->GetSceneDesc().screenSquareCenter = centerSpace;
	GAME->GetSceneDesc().screenSquareMin = ImVec2(centerSpace.x - 10.f, centerSpace.y - 10.f);
	GAME->GetSceneDesc().screenSquareMax = ImVec2(centerSpace.x + 10.f, centerSpace.y + 10.f);

	//ComputeCameraRay(gContext.mRayOrigin, gContext.mRayVector);
}


void EditorTool::DrawTranslationGizmo(int32 type,  OPERATION op)
{
	ImU32 colors[7];
	ComputeColors(colors, type, TRANSLATE);

	ImVec2 origin = GUI->WorldToScreenPos(_model.Translation(), _vp);
	const float worldLength = 0.5f;

	Vec3 directions[3] = { Vec3::Right, Vec3::Up, Vec3::Backward };

	for (int i = 0; i < 3; ++i)
	{

		// 각 축의 뷰포트 변환을 통한 끝점 계산
		Vec3 worldAxisEnd = _model.Translation() + directions[i] * worldLength;
		ImVec2 screenAxisEnd = GUI->WorldToScreenPos(worldAxisEnd, _vp);

		ImVec2 screenAxisVector = ImVec2(screenAxisEnd.x - origin.x, screenAxisEnd.y - origin.y);
		float screenAxisLength = std::sqrt(screenAxisVector.x * screenAxisVector.x + screenAxisVector.y * screenAxisVector.y);

		if (screenAxisLength != 0)
		{
			screenAxisVector.x /= screenAxisLength;
			screenAxisVector.y /= screenAxisLength;

			const float desiredScreenLength = 75.f; 
			ImVec2 endPos = ImVec2(
				origin.x + screenAxisVector.x * desiredScreenLength,
				origin.y + screenAxisVector.y * desiredScreenLength
			);

			GAME->GetSceneDesc().drawList->AddLine(origin, endPos, colors[i + 1], GAME->GetSceneDesc().style.TranslationLineThickness);
			
			ImVec2 dir = ImVec2(origin.x - endPos.x, origin.y - endPos.y);
			float d = std::sqrt(ImLengthSqr(dir));
			dir = dir / d * GAME->GetSceneDesc().style.TranslationLineArrowSize;

			ImVec2 orthogonalDir(dir.y, -dir.x);
			ImVec2 a = endPos + dir;
			GAME->GetSceneDesc().drawList->AddTriangleFilled(endPos - dir, a + orthogonalDir, a - orthogonalDir, colors[i + 1]);
		
		}

		Vec3 planeAxis1 = directions[(i + 1) % 3];
		Vec3 planeAxis2 = directions[(i + 2) % 3];

		ImVec2 screenQuadPts[4];
		for (int j = 0; j < 4; ++j)
		{
			Vec3 cornerWorldPos = _model.Translation() +
				(planeAxis1 * (_quadUV[j * 2] - 0.5f) * worldLength * 2.0f) +
				(planeAxis2 * (_quadUV[j * 2 + 1] - 0.5f) * worldLength * 2.0f);

			screenQuadPts[j] = GUI->WorldToScreenPos(cornerWorldPos, _vp);
		}

		GAME->GetSceneDesc().drawList->AddPolyline(screenQuadPts, 4, GUI->GetColorU32(DIRECTION_X + i), true, 1.0f);
		GAME->GetSceneDesc().drawList->AddConvexPolyFilled(screenQuadPts, 4, colors[i + 4]);
	}

	GAME->GetSceneDesc().drawList->AddCircleFilled(origin, GAME->GetSceneDesc().style.CenterCircleSize, colors[0], 32);
}

void EditorTool::ComputeTripodAxisAndVisibility(const Matrix& mvp, const int axisIndex, Vec3& dirAxis, Vec3& dirPlaneX, Vec3& dirPlaneY, bool& belowAxisLimit, bool& belowPlaneLimit, const bool localCoordinates /*= false*/)
{
	Vec3 direction[3] = { Vec3::Right, Vec3::Up , Vec3::Backward };

	dirAxis = direction[axisIndex];
	dirPlaneX = direction[(axisIndex + 1) % 3];
	dirPlaneY = direction[(axisIndex + 2) % 3];

	float screenFactor = GAME->GetSceneDesc().screenFactor;

	float lenDir = GetSegmentLengthClipSpace(Vec3::Zero, dirAxis);
	float lenDirMinus = GetSegmentLengthClipSpace(Vec3::Zero, -dirAxis);

	float lenDirPlaneX = GetSegmentLengthClipSpace(Vec3::Zero, dirPlaneX);
	float lenDirMinusPlaneX = GetSegmentLengthClipSpace(Vec3::Zero, -dirPlaneX);

	float lenDirPlaneY = GetSegmentLengthClipSpace(Vec3::Zero, dirPlaneY);
	float lenDirMinusPlaneY = GetSegmentLengthClipSpace(Vec3::Zero, -dirPlaneY);
	// For readability
	bool allowFlip = false;
	float mulAxis = (allowFlip && lenDir < lenDirMinus && fabsf(lenDir - lenDirMinus) > FLT_EPSILON) ? -1.f : 1.f;
	float mulAxisX = (allowFlip && lenDirPlaneX < lenDirMinusPlaneX && fabsf(lenDirPlaneX - lenDirMinusPlaneX) > FLT_EPSILON) ? -1.f : 1.f;
	float mulAxisY = (allowFlip && lenDirPlaneY < lenDirMinusPlaneY && fabsf(lenDirPlaneY - lenDirMinusPlaneY) > FLT_EPSILON) ? -1.f : 1.f;
	dirAxis *= mulAxis;
	dirPlaneX *= mulAxisX;
	dirPlaneY *= mulAxisY;

	// for axis
	float axisLengthInClipSpace = GetSegmentLengthClipSpace(Vec3::Zero, dirAxis * screenFactor);

	float paraSurf = GetParallelogram(mvp, Vec3::Zero, dirPlaneX * screenFactor, dirPlaneY * screenFactor);
	belowPlaneLimit = (paraSurf > GAME->GetSceneDesc().axisLimit);
	belowAxisLimit = (axisLengthInClipSpace > GAME->GetSceneDesc().planeLimit);

	// and store values
	GAME->GetSceneDesc().axisFactor[axisIndex] = mulAxis;
	GAME->GetSceneDesc().axisFactor[(axisIndex + 1) % 3] = mulAxisX;
	GAME->GetSceneDesc().axisFactor[(axisIndex + 2) % 3] = mulAxisY;
	GAME->GetSceneDesc().belowAxisLimit[axisIndex] = belowAxisLimit;
	GAME->GetSceneDesc().belowPlaneLimit[axisIndex] = belowPlaneLimit;
}

float EditorTool::GetParallelogram(const Matrix& mvp, const Vec3& ptO, const Vec3& ptA, const Vec3& ptB)
{

	Vec3 pts[] = { ptO, ptA, ptB };
	for (int i = 0; i < 3; i++)
	{
		Vec3 ptVec = XMLoadFloat3(&pts[i]);
		ptVec = XMVector4Transform(ptVec, mvp);

		if (fabsf(XMVectorGetW(ptVec)) > FLT_EPSILON)
		{
			ptVec = XMVectorScale(ptVec, 1.f / XMVectorGetW(ptVec));
		}

		XMStoreFloat3(&pts[i], ptVec);
	}

	Vec3 segA = Vec3(pts[1].x - pts[0].x, pts[1].y - pts[0].y, pts[1].z - pts[0].z);
	Vec3 segB = Vec3(pts[2].x - pts[0].x, pts[2].y - pts[0].y, pts[2].z - pts[0].z);

	segA.y /= GAME->GetSceneDesc().displayRatio;
	segB.y /= GAME->GetSceneDesc().displayRatio;

	Vec3 segAOrtho = Vec3(-segA.y, segA.x, 0.0f);
	segAOrtho.Normalize();

	Vec3 segBVec = Vec3(segB.x, segB.y, 0.0f);
	Vec3 segAOrthoVec = XMLoadFloat3(&segAOrtho);
	Vec3 segBVecVec = XMLoadFloat3(&segBVec);

	float dt;
	XMStoreFloat(&dt, XMVector3Dot(segAOrthoVec, segBVecVec));

	float surface = sqrtf(segA.x * segA.x + segA.y * segA.y) * fabsf(dt);
	return surface;
}
//
float EditorTool::GetSegmentLengthClipSpace(const Vec3& start, const Vec3& end)
{
	shared_ptr<GameObject> selectedGo = CUR_SCENE->GetCreatedObject(TOOL->GetSelectedIdH());

	if (selectedGo == nullptr)
		return 0.f;

	shared_ptr<Transform> tr = selectedGo->GetTransform();

	Vec4 startOfSegment;

	Vec3 startVec = XMLoadFloat3(&start);
	Vec3 endVec = XMLoadFloat3(&end);

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();

	startOfSegment = Vec3::Transform(startVec, tr->GetWorldMatrix() * cam->GetViewMatrix() * cam->GetProjectionMatrix());

	// Check for axis aligned with camera direction
	if (fabsf(XMVectorGetW(startOfSegment)) > FLT_EPSILON)
	{
		startOfSegment = XMVectorScale(startOfSegment, 1.f / XMVectorGetW(startOfSegment));
	}

	Vec4 endOfSegment;
	endOfSegment = XMVector4Transform(endVec, tr->GetWorldMatrix() * cam->GetViewMatrix() * cam->GetProjectionMatrix());

	// Check for axis aligned with camera direction
	if (fabsf(XMVectorGetW(endOfSegment)) > FLT_EPSILON)
	{
		endOfSegment = XMVectorScale(endOfSegment, 1.f / XMVectorGetW(endOfSegment));
	}

	XMVECTOR clipSpaceAxis = XMVectorSubtract(endOfSegment, startOfSegment);

	if (GAME->GetSceneDesc().displayRatio < 1.0)
	{
		clipSpaceAxis = XMVectorScale(clipSpaceAxis, static_cast<float>(GAME->GetSceneDesc().displayRatio));
	}
	else
	{
		clipSpaceAxis = XMVectorScale(clipSpaceAxis, 1.0f / static_cast<float>(GAME->GetSceneDesc().displayRatio));
	}

	XMFLOAT2 clipSpaceAxis2D;
	XMStoreFloat2(&clipSpaceAxis2D, clipSpaceAxis);

	float segmentLengthInClipSpace = sqrtf(clipSpaceAxis2D.x * clipSpaceAxis2D.x + clipSpaceAxis2D.y * clipSpaceAxis2D.y);
	return segmentLengthInClipSpace;
}

void EditorTool::DrawHatchedAxis(const Vec3& axis)
{
	shared_ptr<GameObject> selectedGo = CUR_SCENE->GetCreatedObject(TOOL->GetSelectedIdH());

	if (selectedGo == nullptr)
		return;

	shared_ptr<Transform> tr = selectedGo->GetTransform();

	float screenFactor = GAME->GetSceneDesc().screenFactor;
	Matrix mvp=  tr->GetWorldMatrix() * _view * _projection;
	
	for (int j = 1; j < 10; j++)
	{
		ImVec2 baseSpace = GUI->WorldToScreenPos(axis * 0.05f * (float)(j * 2) * screenFactor, mvp);
		ImVec2 wordDirSpace = GUI->WorldToScreenPos(axis * 0.05f * (float)(j * 2 + 1) * screenFactor, mvp);
		GAME->GetSceneDesc().drawList->AddLine(baseSpace, wordDirSpace, IM_COL32(255, 0, 0, 255), 1);
	}
}
