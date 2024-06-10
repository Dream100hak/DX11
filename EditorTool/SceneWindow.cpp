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

#include "Model.h"

const char* SceneWindow::s_translationInfoMask[] = { "X : %5.3f", "Y : %5.3f", "Z : %5.3f",
   "Y : %5.3f Z : %5.3f", "X : %5.3f Z : %5.3f", "X : %5.3f Y : %5.3f",
   "X : %5.3f Y : %5.3f Z : %5.3f" };

const char* SceneWindow::s_scaleInfoMask[] = { "X : %5.2f", "Y : %5.2f", "Z : %5.2f", "XYZ : %5.2f" };
const int SceneWindow::s_translationInfoIndex[] = { 0,0,0, 1,0,0, 2,0,0, 1,2,0, 0,2,0, 0,1,0, 0,1,2 };


SceneWindow::SceneWindow(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos , size);
}

SceneWindow::~SceneWindow()
{

}

void SceneWindow::Init()
{

}

void SceneWindow::Update()
{
	ShowSceneWindow();
}

void SceneWindow::ShowSceneWindow()
{

	const ImGuiIO& io = ImGui::GetIO();

	auto& prviewObjs = 
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshPreviewObjs();

	auto& scales =
		static_pointer_cast<FolderContents>(TOOL->GetEditorWindow(Utils::GetClassNameEX<FolderContents>()))->GetMeshScales();

	ImVec2 scenePos(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
	ImVec2 sceneSize(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height);

	if (ImGui::BeginDragDropTargetCustom(ImRect(scenePos, scenePos + sceneSize), ImGui::GetID("Scene"))) 
	{	
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MeshPayload"))
		{
			MetaData** droppedMeshRawPtr = static_cast<MetaData**>(payload->Data);
			shared_ptr<MetaData> droppedMesh =	make_shared<MetaData>(**droppedMeshRawPtr);

			shared_ptr<GameObject> obj =  prviewObjs[droppedMesh->fileFullPath + L"/" + droppedMesh->fileName];
			CUR_SCENE->Remove(obj);
		
			shared_ptr<Model> model = make_shared<Model>();
			wstring modelName = droppedMesh->fileName.substr(0, droppedMesh->fileName.find('.'));
			model->ReadModel(modelName + L'/' + modelName);
			model->ReadMaterialByXml(modelName + L'/' + modelName);

			int32 id = GUI->CreateModelMesh(model , obj->GetTransform()->GetPosition());
			CUR_SCENE->UnPickAll();
			TOOL->SetSelectedObjH(id);
			
			shared_ptr<GameObject> makeObj = CUR_SCENE->GetCreatedObject(id);
			CUR_SCENE->GetCreatedObject(id)->SetUIPicked(true);

			//float scale = scales[droppedMesh->fileFullPath + L"/" + droppedMesh->fileName];
			//CUR_SCENE->GetCreatedObject(id)->GetTransform()->SetScale(Vec3(scale * 6));

			ADDLOG("Create Object : " + Utils::ToString(droppedMesh->fileName) , LogFilter::Warn);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}
		ImGui::EndDragDropTarget();
	}


	EditTransform();

	if (_bUsing == false)
	{
		if (io.MouseClicked[0])
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
}

void SceneWindow::EditTransform()
{
	if(_tr == nullptr)
		return;

	if(_tr->GetGameObject() == nullptr || _tr->GetGameObject()->IsIgnoredTransformEdit())
		return;

	ImGuiIO& io = ImGui::GetIO();

	static SceneWindow::Mode currentGizmoMode(SceneWindow::Local);
	static bool useSnap = false;
	static float snap[3] = { 1.f, 1.f, 1.f };
	static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	if (ImGui::IsKeyPressed(ImGuiKey_T))
		_currentGizmoOperation = TRANSLATE;
	if (ImGui::IsKeyPressed(ImGuiKey_E))
		_currentGizmoOperation = ROTATE;
	if (ImGui::IsKeyPressed(ImGuiKey_R)) // r Key
		_currentGizmoOperation = SCALE;
	
	
	/*if (ImGui::RadioButton("Translate", _currentGizmoOperation == TRANSLATE))
		_currentGizmoOperation = TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", _currentGizmoOperation == ROTATE))
		_currentGizmoOperation = ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", _currentGizmoOperation == SCALE))
		_currentGizmoOperation = SCALE;

	int32 mouse[2] = { io.MousePos.x , io.MousePos.y };
	ImGui::InputInt2("Mouse Pos : ", mouse);
	*/
	uint32 fps = GET_SINGLE(TimeManager)->GetFps();
	char tmps[64];
	ImFormatString(tmps, sizeof(tmps),"FPS : %d", fps);
	ImGui::Text(tmps);



	//if (ImGui::RadioButton("Universal", _currentGizmoOperation == UNIVERSAL))
	//	_currentGizmoOperation = UNIVERSAL;

	//float matrixTranslation[3] = { _tr->GetLocalPosition().x , _tr->GetLocalPosition().y , _tr->GetLocalPosition().z };
	//float matrixRotation[3] = { _tr->GetLocalRotation().x , _tr->GetLocalRotation().y , _tr->GetLocalRotation().z };
	//float matrixScale[3] = { _tr->GetLocalScale().x , _tr->GetLocalScale().y , _tr->GetLocalScale().z };

	//ImGui::InputFloat3("Tr", matrixTranslation);
	//ImGui::InputFloat3("Rt", matrixRotation);
	//ImGui::InputFloat3("Sc", matrixScale);


	//if (_currentGizmoOperation != SCALE)
	//{
	//	if (ImGui::RadioButton("Local", currentGizmoMode == Local))
	//		currentGizmoMode = Local;
	//	ImGui::SameLine();
	//	if (ImGui::RadioButton("World", currentGizmoMode == World))
	//		currentGizmoMode = World;
	//}
	//if (ImGui::IsKeyPressed(ImGuiKey_S))
	//	useSnap = !useSnap;
	//ImGui::Checkbox("##UseSnap", &useSnap);
	//ImGui::SameLine();

	//switch (_currentGizmoOperation)
	//{
	//case TRANSLATE:
	//	ImGui::InputFloat3("Snap", &snap[0]);
	//	break;
	//case ROTATE:
	//	ImGui::InputFloat("Angle Snap", &snap[0]);
	//	break;
	//case SCALE:
	//	ImGui::InputFloat("Scale Snap", &snap[0]);
	//	break;
	//}
	//ImGui::Checkbox("Bound Sizing", &boundSizing);
	//if (boundSizing)
	//{

	//	ImGui::Checkbox("##BoundSizing", &boundSizingSnap);
	//	ImGui::SameLine();
	//	ImGui::InputFloat3("Snap", boundsSnap);
	//}

	Manipulate( _currentGizmoOperation, currentGizmoMode, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

}

void SceneWindow::Manipulate(OPERATION operation, Mode mode, const float* snap, const float* localBounds, const float* boundsSnap)
{
	ComputeContext((operation & SCALE) ? Local : mode);

	int type = MT_NONE;

	HandleTranslation(operation, type, mode, nullptr);
	HandleScale(operation, type, mode, nullptr);

	DrawTranslationGizmo(operation, type);
	DrawScaleGizmo(operation , type);

}

void SceneWindow::HandleTranslation(OPERATION op, int& type, Mode mode, const float* snap)
{
	if (!Intersects(op, TRANSLATE) || type != MT_NONE)
		return;
	
	const ImGuiIO& io = ImGui::GetIO();
	const bool applyRotationLocaly = mode == Local || type == MT_MOVE_SCREEN;

	// move
	if (_bUsing && IsTranslateType(_currentOperation))
	{
		ImGui::SetNextFrameWantCaptureMouse(true);

		float signedLength =  IntersectRayPlane(_rayOrigin, _rayDir, _translationPlan);
		float len = fabsf(signedLength);
		Vec4 newPos = _rayOrigin + _rayDir * len;
		
		// compute delta
		const Vec4 newOrigin = newPos - _relativeOrigin * _screenFactor;
		Vec3 delta = newOrigin - _model.Translation();

		// 1 axis constraint
		if (_currentOperation >= MT_MOVE_X && _currentOperation <= MT_MOVE_Z)
		{
			const int axisIndex = _currentOperation - MT_MOVE_X;
			const Vec3& axisValue = *(Vec3*)&_model.m[axisIndex];
			const float lengthOnAxis = axisValue.Dot(delta);
			delta = axisValue * lengthOnAxis;
		}

		XMMATRIX deltaMatrixTranslation = XMMatrixTranslation(delta.x, delta.y, delta.z);
	
		XMMATRIX res = XMLoadFloat4x4(&_modelSource) * deltaMatrixTranslation;
		XMFLOAT4X4 newMatrix;
		XMStoreFloat4x4(&newMatrix, res);
		_tr->SetPosition(Vec3(newMatrix._41, newMatrix._42 , newMatrix._43));

		if (!io.MouseDown[0])
			_bUsing = false; 
		

		type = _currentOperation;
	}
	else
	{
		Vec3 gizmoHitProportion = Vec3::Zero;
		type = GetMoveType(op, gizmoHitProportion);
		if (type != MT_NONE)
		{
			ImGui::SetNextFrameWantCaptureMouse(true);
		}
		if (GUI->CanActivate() && type != MT_NONE)
		{
			_bUsing = true;
			_currentOperation = type;

			Vec3 movePlanNormal[] = { _model.Translation().Right, _model.Up(), _model.Backward(),
							  _model.Translation().Right, _model.Translation().Up, _model.Translation().Backward,
							  -MAIN_CAM->GetTransform()->GetLook() };

			Vec3 cameraToModelNormalized = _model.Translation() - MAIN_CAM->GetTransform()->GetLocalPosition();
			cameraToModelNormalized.Normalize();

			for (uint32 i = 0; i < 3; i++)
			{
				Vec3 orthoVector = movePlanNormal[i].Cross(cameraToModelNormalized);
				movePlanNormal[i] = orthoVector.Cross(movePlanNormal[i]);
				movePlanNormal[i].Normalize();
			}

			// pickup plan
			_translationPlan = BuildPlan( _model.Translation() , movePlanNormal[type - MT_MOVE_X]);
			 float len = IntersectRayPlane(_rayOrigin, _rayDir, _translationPlan);
			_translationPlanOrigin = _rayOrigin + _rayDir * len;
			_matrixOrigin = _model.Translation();
		
			_relativeOrigin = (_translationPlanOrigin - _model.Translation()) * (1.f / _screenFactor);
		}
	}
}

void SceneWindow::HandleScale(OPERATION op, int& type, Mode mode, const float* snap)
{
	if ((!Intersects(op, SCALE) && !Intersects(op, SCALEU)) || type != MT_NONE || !GUI->IsHoveringWindow())
		return;
	
	ImGuiIO& io = ImGui::GetIO();

	if (!_bUsing)
	{
		type = GetScaleType(op);
		if (type != MT_NONE)
		{
			ImGui::SetNextFrameWantCaptureMouse(true);
		}
		if (GUI->CanActivate() && type != MT_NONE)
		{
			_bUsing = true;
			_currentOperation = type;
			Vec3 movePlanNormal[] = { _model.Translation().Up, _model.Translation().Backward, _model.Translation().Right,_model.Translation().Backward, _model.Translation().Up, _model.Translation().Right, -MAIN_CAM->GetTransform()->GetLook() };
			// pickup plan

			_translationPlan = BuildPlan(_model.Translation(), movePlanNormal[type - MT_SCALE_X]);
			float len = IntersectRayPlane(_rayOrigin, _rayDir, _translationPlan);
			_translationPlanOrigin = _rayOrigin + _rayDir * len;
			_matrixOrigin = _model.Translation();
			_relativeOrigin = (_translationPlanOrigin - _model.Translation()) * (1.f / _screenFactor);
			_scaleValueOrigin = Vec3(_modelSource.Right().Length(), _modelSource.Up().Length(), _modelSource.Backward().Length());
			_saveMousePosX = io.MousePos.x;
		}
	}
	// scale
	if (_bUsing && IsScaleType(_currentOperation))
	{

		ImGui::SetNextFrameWantCaptureMouse(true);

		const float len = IntersectRayPlane(_rayOrigin, _rayDir, _translationPlan);
		Vec4 newPos = _rayOrigin + _rayDir * len;
		Vec4 newOrigin = newPos - _relativeOrigin * _screenFactor;
		Vec3 delta = newOrigin - _modelLocal.Translation();
		Vec3 newScale = _scaleValueOrigin;
		// 1 axis constraint
		if (_currentOperation >= MT_SCALE_X && _currentOperation <= MT_SCALE_Z)
		{
			int axisIndex = _currentOperation - MT_SCALE_X;
			const Vec3& axisValue = *(Vec3*)&_modelLocal.m[axisIndex];
			float lengthOnAxis = Dot3(axisValue, delta);
			delta = axisValue * lengthOnAxis;

			Vec3 baseVector = _translationPlanOrigin - _modelLocal.Translation();
			float ratio = Dot3(axisValue, baseVector + delta) / Dot3(axisValue, baseVector);
		
			((float*)&newScale)[axisIndex] = max(ratio * ((float*)&_scaleValueOrigin)[axisIndex], 0.001f);
			_tr->SetLocalScale(newScale);
		}
		else
		{
			float scaleDelta = (io.MousePos.x - _saveMousePosX) * 0.01f;
			newScale = _scaleValueOrigin * max(1.f + scaleDelta, 0.001f);
			_tr->SetLocalScale(newScale);
		}

		if (!io.MouseDown[0])
			_bUsing = false;
		

		type = _currentOperation;
	}
}

void SceneWindow::ComputeContext( Mode mode)
{
	if (_tr == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();

	_view = MAIN_CAM->GetViewMatrix();
	_projection = MAIN_CAM->GetProjectionMatrix();

	_modelLocal = _tr->GetLocalMatrix();
	_modelLocal._11 = _modelLocal._22 = _modelLocal._33 = 1;

	if (mode == Local)
		_model = _modelLocal;
	else
		_model = _tr->GetWorldMatrix();
	
	_modelSource = _tr->GetWorldMatrix();
	_modelScaleOrigin = Vec3(_modelSource.Right().Length(), _modelSource.Up().Length(), _modelSource.Backward().Length());

	_modelInverse = _model;
	_modelInverse.Invert();
	_modelSourceInverse = _modelSource;
	_modelSourceInverse.Invert();

	_vp = _view * _projection;
	_mvp = _model * _vp;

	Matrix viewInverse = _view;
	viewInverse.Invert();

	// projection reverse
	Vec3 rightViewInverse = MAIN_CAM->GetTransform()->GetRight();

	rightViewInverse = Vec3::TransformNormal(rightViewInverse, _modelInverse);
	float rightLength = GetSegmentLengthClipSpace(Vec3::Zero, rightViewInverse);
	_screenFactor = _gizmoSizeClipSpace / rightLength;

	ImVec2 centerSSpace = GUI->WorldToScreenPos(Vec3::Zero, _mvp);
	_screenSquareCenter = centerSSpace;
	_screenSquareMin = ImVec2(centerSSpace.x - 10.f, centerSSpace.y - 10.f);
	_screenSquareMax = ImVec2(centerSSpace.x + 10.f, centerSSpace.y + 10.f);

	ComputeCameraRay(_rayOrigin , _rayDir);

}

float SceneWindow::IntersectRayPlane(const XMVECTOR& rOrigin, const XMVECTOR& rVector, const Vec4& plane)
{
	float numer = Dot3(plane , rOrigin) - plane.w;
	float denom = Dot3(plane, rVector);

	if (fabsf(denom) < FLT_EPSILON) {
		return -1.0f;  // 평면과 레이가 평행하면 교차점 없음
	}

	return -(numer / denom);
}
//
Plane SceneWindow::BuildPlan(const Vec3& pointOrigin, Vec3& normalOrigin)
{
	Plane res;
	Vec3 normal = normalOrigin;
	normal.Normalize();

	res.w = Dot3(normal, pointOrigin);
	res.x = normal.x;
	res.y = normal.y;
	res.z = normal.z;
	return res;
}

void SceneWindow::DrawHatchedAxis(const Vec3& axis)
{
	if (GAME->GetSceneDesc().style.HatchedAxisLineThickness <= 0.0f)
		return;
	
	for (int j = 1; j < 10; j++)
	{
		ImVec2 baseSSpace2 = GUI->WorldToScreenPos(axis * 0.05f * (float)(j * 2) * _screenFactor, _mvp);
		ImVec2 worldDirSSpace2 = GUI->WorldToScreenPos(axis * 0.05f * (float)(j * 2 + 1) * _screenFactor, _mvp);
		GAME->GetSceneDesc().drawList->AddLine(baseSSpace2, worldDirSSpace2, GUI->GetColorU32(HATCHED_AXIS_LINES), GAME->GetSceneDesc().style.HatchedAxisLineThickness);
	}
}

ImVec2 SceneWindow::PointOnSegment(const ImVec2& point, const ImVec2& vertPos1, const ImVec2& vertPos2)
{
	Vec2 pos0 = Vec2(point.x, point.y);
	Vec2 pos1 = Vec2(vertPos1.x, vertPos1.y);
	Vec2 pos2 = Vec2(vertPos2.x, vertPos2.y);

	Vec2 originV = pos2 - pos1;
	Vec2 v = pos2 - pos1;
	v.Normalize();

	float d = originV.Length();
	float t = Dot3(v, pos0 - pos1);

	if (t < 0.f)
		return vertPos1;
	
	if (t > d)
		return vertPos2;
	
	return vertPos1 + ImVec2(v.x, v.y) * t;
}

void SceneWindow::ComputeCameraRay(Vec4& rayOrigin, Vec4& rayDir)
{

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 position = ImVec2(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
	ImVec2 size = ImVec2(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height);

	float mox = ((io.MousePos.x - position.x) / size.x) * 2.f - 1.f;
	float moy = (1.f - ((io.MousePos.y - position.y) / size.y)) * 2.f - 1.f;

	float zNear = 0.f;
	float zFar = 1.f - FLT_EPSILON;

	Vec4 nearPoint = Vec4(mox, moy, zNear, 1.f);
	Vec4 farPoint = Vec4(mox, moy, zFar, 1.f);

	rayOrigin = Vec4::Transform(nearPoint, _vp.Invert());
	rayOrigin *= 1.f / rayOrigin.w;

	Vec4 rayEnd = Vec4::Transform(farPoint , _vp.Invert());
	rayEnd *= 1.f / rayEnd.w;
	rayDir = rayEnd - rayOrigin;
	rayDir.Normalize();
}

int32 SceneWindow::GetMoveType(OPERATION op, Vec3& gizmoHitProportion)
{
	if (!Intersects(op, TRANSLATE) || _bUsing ||   !GUI->IsHoveringWindow())
		return MT_NONE;
	

	int type = MT_NONE;

	ImGuiIO& io = ImGui::GetIO();

	// screen
	if (io.MousePos.x >= _screenSquareMin.x && io.MousePos.x <= _screenSquareMax.x &&
		io.MousePos.y >= _screenSquareMin.y && io.MousePos.y <= _screenSquareMax.y &&
		Contains(op, TRANSLATE))
	{
		type = MT_MOVE_SCREEN;
	}

	ImVec2 screenCoordGUI = io.MousePos - ImVec2(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);

	// compute
	for (int i = 0; i < 3 && type == MT_NONE; i++)
	{
		Vec3 dirPlaneX, dirPlaneY, dirAxis;
		bool belowAxisLimit, belowPlaneLimit;

		ComputeTripodAxisAndVisibility(i, dirAxis, dirPlaneX, dirPlaneY, belowAxisLimit, belowPlaneLimit);
		dirAxis = Vec3::TransformNormal(dirAxis, _model);
		dirPlaneX = Vec3::TransformNormal(dirPlaneX, _model);
		dirPlaneY = Vec3::TransformNormal(dirPlaneY, _model);
		float len = 0.f;
		Vec4 posOnPlan = Vec4::Zero;
		Vec3 trans = _model.Translation();

		len = IntersectRayPlane(_rayOrigin , _rayDir, BuildPlan(trans, dirAxis));
		posOnPlan = _rayOrigin + _rayDir * len;

		ImVec2 axisStartOnScreen = GUI->WorldToScreenPos(_model.Translation() + dirAxis * _screenFactor * 0.1f, _vp) - ImVec2(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);
		ImVec2 axisEndOnScreen = GUI->WorldToScreenPos(_model.Translation() + dirAxis * _screenFactor, _vp) - ImVec2(GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);

		ImVec2 closestPointOnAxis = PointOnSegment(screenCoordGUI, axisStartOnScreen, axisEndOnScreen);
		ImVec2 calcPointGUI = closestPointOnAxis - screenCoordGUI;
		Vec2 calcPoint = Vec2(calcPointGUI.x , calcPointGUI.y);

		if (calcPoint.Length() < 16.f && Intersects(op, static_cast<OPERATION>(TRANSLATE_X << i))) // pixel size
		{		
			type = MT_MOVE_X + i;
		}

		float dx = Dot3(dirPlaneX , (posOnPlan - _model.Translation()) * (1.f / _screenFactor));
		float dy = Dot3(dirPlaneY , (posOnPlan - _model.Translation()) * (1.f / _screenFactor));

		if (belowPlaneLimit && dx >= _quadUV[0] && dx <= _quadUV[4] && dy >= _quadUV[1] && dy <= _quadUV[3] && Contains(op, TRANSLATE_PLANS[i]))
		{
			type = MT_MOVE_YZ + i;
		}

		gizmoHitProportion = Vec3(dx, dy, 0.f);
		
	}
	return type;
}

int32 SceneWindow::GetScaleType(OPERATION op)
{
	if (_bUsing)
	{
		return MT_NONE;
	}
	ImGuiIO& io = ImGui::GetIO();
	int type = MT_NONE;

	// screen
	if (io.MousePos.x >= _screenSquareMin.x && io.MousePos.x <= _screenSquareMax.x &&
		io.MousePos.y >= _screenSquareMin.y && io.MousePos.y <= _screenSquareMax.y &&
		Contains(op, SCALE))
	{
		type = MT_SCALE_XYZ;
	}

	// compute
	for (int i = 0; i < 3 && type == MT_NONE; i++)
	{
		if (!Intersects(op, static_cast<OPERATION>(SCALE_X << i)))
		{
			continue;
		}
		Vec3 dirPlaneX, dirPlaneY, dirAxis;
		bool belowAxisLimit, belowPlaneLimit;
		ComputeTripodAxisAndVisibility(i, dirAxis, dirPlaneX, dirPlaneY, belowAxisLimit, belowPlaneLimit, true);
		dirAxis = Vec3::TransformNormal(dirAxis, _modelLocal);
		dirPlaneX = Vec3::TransformNormal(dirPlaneX, _modelLocal);
		dirPlaneY = Vec3::TransformNormal(dirPlaneY, _modelLocal);

		const float len = IntersectRayPlane(_rayOrigin, _rayDir, BuildPlan(_modelLocal.Translation(), dirAxis));
		XMVECTOR posOnPlan = _rayOrigin + _rayDir * len;

		const float startOffset = Contains(op, static_cast<OPERATION>(TRANSLATE_X << i)) ? 1.0f : 0.1f;
		const float endOffset = Contains(op, static_cast<OPERATION>(TRANSLATE_X << i)) ? 1.4f : 1.0f;
		const ImVec2 posOnPlanScreen = GUI->WorldToScreenPos(posOnPlan, _vp);
		const ImVec2 axisStartOnScreen = GUI->WorldToScreenPos(_modelLocal.Translation() + dirAxis * _screenFactor * startOffset, _vp);
		const ImVec2 axisEndOnScreen = GUI->WorldToScreenPos(_modelLocal.Translation() + dirAxis * _screenFactor * endOffset, _vp);

		ImVec2 closestPointOnAxis = PointOnSegment(ImVec2(posOnPlanScreen), ImVec2(axisStartOnScreen), ImVec2(axisEndOnScreen));
		Vec2  closestPoint = Vec2(closestPointOnAxis.x - posOnPlanScreen.x, closestPointOnAxis.y - posOnPlanScreen.y);

		if (closestPoint.Length() < 12.f) // pixel size
		{
			type = MT_SCALE_X + i;
		}
	}

	// universal

	Vec4 deltaScreen = { io.MousePos.x - _screenSquareCenter.x, io.MousePos.y - _screenSquareCenter.y, 0.f, 0.f };
	float dist = deltaScreen.Length();
	if (Contains(op, SCALEU) && dist >= 17.0f && dist < 23.0f)
	{
		type = MT_SCALE_XYZ;
	}

	for (int i = 0; i < 3 && type == MT_NONE; i++)
	{
		if (!Intersects(op, static_cast<OPERATION>(SCALE_XU << i)))
		{
			continue;
		}

		Vec3 dirPlaneX, dirPlaneY, dirAxis;
		bool belowAxisLimit, belowPlaneLimit;
		ComputeTripodAxisAndVisibility(i, dirAxis, dirPlaneX, dirPlaneY, belowAxisLimit, belowPlaneLimit, true);

		// draw axis
		if (belowAxisLimit)
		{
			bool hasTranslateOnAxis = Contains(op, static_cast<OPERATION>(TRANSLATE_X << i));
			float markerScale = hasTranslateOnAxis ? 1.4f : 1.0f;
			ImVec2 worldDirSSpace = GUI->WorldToScreenPos((dirAxis * markerScale) * _screenFactor, _modelLocal * _vp);

			float distance = sqrtf(ImLengthSqr(worldDirSSpace - io.MousePos));
			if (distance < 12.f)
			{
				type = MT_SCALE_X + i;
			}
		}
	}
	return type;
}

void SceneWindow::ComputeTripodAxisAndVisibility(const int axisIndex, Vec3& dirAxis, Vec3& dirPlaneX, Vec3& dirPlaneY, bool& belowAxisLimit, bool& belowPlaneLimit, const bool localCoordinates)
 {
	 Vec3 directions[3] = { Vec3::Left, Vec3::Up, Vec3::Backward };
	 dirAxis = directions[axisIndex];
	 dirPlaneX = directions[(axisIndex + 1) % 3];
	 dirPlaneY = directions[(axisIndex + 2) % 3];

	 if (_bUsing)
	 {
		 belowAxisLimit = _belowAxisLimit[axisIndex];
		 belowPlaneLimit = _belowAxisLimit[axisIndex];

		 dirAxis *= _axisFactor[axisIndex];
		 dirPlaneX *= _axisFactor[(axisIndex + 1) % 3];
		 dirPlaneY *= _axisFactor[(axisIndex + 2) % 3];
	 }
	 else
	 {
		 // new method
		 float lenDir = GetSegmentLengthClipSpace(Vec3::Zero, dirAxis, localCoordinates);
		 float lenDirMinus = GetSegmentLengthClipSpace(Vec3::Zero, -dirAxis, localCoordinates);

		 float lenDirPlaneX = GetSegmentLengthClipSpace(Vec3::Zero, dirPlaneX, localCoordinates);
		 float lenDirMinusPlaneX = GetSegmentLengthClipSpace(Vec3::Zero, -dirPlaneX, localCoordinates);

		 float lenDirPlaneY = GetSegmentLengthClipSpace(Vec3::Zero, dirPlaneY, localCoordinates);
		 float lenDirMinusPlaneY = GetSegmentLengthClipSpace(Vec3::Zero, -dirPlaneY, localCoordinates);

		 // For readability
		 bool& allowFlip = _allowFlip;
		 float mulAxis = (allowFlip && lenDir < lenDirMinus && fabsf(lenDir - lenDirMinus) > FLT_EPSILON) ? -1.f : 1.f;
		 float mulAxisX = (allowFlip && lenDirPlaneX < lenDirMinusPlaneX && fabsf(lenDirPlaneX - lenDirMinusPlaneX) > FLT_EPSILON) ? -1.f : 1.f;
		 float mulAxisY = (allowFlip && lenDirPlaneY < lenDirMinusPlaneY && fabsf(lenDirPlaneY - lenDirMinusPlaneY) > FLT_EPSILON) ? -1.f : 1.f;
		
		 dirAxis *= mulAxis;
		 dirPlaneX *= mulAxisX;
		 dirPlaneY *= mulAxisY;

		 // for axis
		 float axisLengthInClipSpace = GetSegmentLengthClipSpace(Vec3::Zero, dirAxis * _screenFactor, localCoordinates);

		 float paraSurf = GetParallelogram(Vec3::Zero, dirPlaneX * _screenFactor, dirPlaneY * _screenFactor);
		 belowPlaneLimit = (paraSurf > _axisLimit);
		 belowAxisLimit = (axisLengthInClipSpace > _planeLimit);

		 // and store values
		 _axisFactor[axisIndex] = mulAxis;
		 _axisFactor[(axisIndex + 1) % 3] = mulAxisX;
		 _axisFactor[(axisIndex + 2) % 3] = mulAxisY;
		 _belowAxisLimit[axisIndex] = belowAxisLimit;
		 _belowPlaneLimit[axisIndex] = belowPlaneLimit;
	 }
 }


 float SceneWindow::GetParallelogram(const Vec3& ptO, const Vec3& ptA, const Vec3& ptB)
 {
	 Vec4 pts[] = { Vec4(ptO.x, ptO.y , ptO.z , 1.f), Vec4(ptA.x, ptA.y , ptA.z , 1.f),  Vec4(ptB.x, ptB.y , ptB.z , 1.f) };
	 for (uint32 i = 0; i < 3; i++)
	 {
		 Vec4 ptsOfSeg = Vec4::Transform(pts[i] , _mvp);
		 if (fabsf(ptsOfSeg.w) > FLT_EPSILON)
			 ptsOfSeg *= 1.f / ptsOfSeg.w;

		pts[i] = ptsOfSeg;
	 }

	 Vec4 segA = pts[1] - pts[0];
	 Vec4 segB = pts[2] - pts[0];
	 segA.y /= _displayRatio;
	 segB.y /= _displayRatio;
	 Vec4 segAOrtho = Vec4(-segA.y, segA.x , 0 , 0);
	 segAOrtho.Normalize();
	 float dt = Dot3(segAOrtho, segB);
	 float surface = sqrtf(segA.x * segA.x + segA.y * segA.y) * fabsf(dt);
	 return surface;

 }

float SceneWindow::GetSegmentLengthClipSpace(const Vec3& start, const Vec3& end, const bool localCoordinates )
{
	Vec4 start4 = Vec4(start.x, start.y , start.z , 1.f);
	const Matrix& mvp = localCoordinates ? _modelLocal : _mvp;

	Vec4 startOfSegment = Vec4::Transform(start4, mvp);

	if (fabsf(startOfSegment.w) > FLT_EPSILON) // check for axis aligned with camera direction
	{
		startOfSegment *= 1.f / startOfSegment.w;
	}

	Vec4 end4 = Vec4(end.x, end.y , end.z , 1.f);
	Vec4 endOfSegment = Vec4::Transform(end4, mvp);

	if (fabsf(endOfSegment.w) > FLT_EPSILON) // check for axis aligned with camera direction
	{
		endOfSegment *= 1.f / endOfSegment.w;
	}

	Vec4 clipSpaceAxis = endOfSegment - startOfSegment;
	if (_displayRatio < 1.0)
		clipSpaceAxis.x *= _displayRatio;
	else
		clipSpaceAxis.y /= _displayRatio;
	float segmentLengthInClipSpace = sqrtf(clipSpaceAxis.x * clipSpaceAxis.x + clipSpaceAxis.y * clipSpaceAxis.y);
	return segmentLengthInClipSpace;
}

void SceneWindow::DrawTranslationGizmo(OPERATION op, int32 type)
{

	if (!Intersects(op, TRANSLATE))
		return;
	
	ImU32 colors[7];
	ComputeColors(colors, type, TRANSLATE);

	const ImVec2 origin = GUI->WorldToScreenPos(_model.Translation(), _vp);
	// draw
	bool belowAxisLimit = false;
	bool belowPlaneLimit = false;


	Vec3 camToGizmoDir = _model.Translation() - MAIN_CAM->GetTransform()->GetLocalPosition();
	camToGizmoDir.Normalize();

	// 카메라 시선 방향
	Vec3 camForwardDir = MAIN_CAM->GetTransform()->GetLook();
	camForwardDir.Normalize();

	// 각도 계산
	float dotProduct = camToGizmoDir.Dot(camForwardDir);
	float angle = acos(dotProduct);

	// 기즈모 가시성 결정
	if (angle >= MathUtils::PI / 2) // 90도 이상이면 return
		return;

	for (int i = 0; i < 3; ++i)
	{
		Vec3 dirPlaneX, dirPlaneY, dirAxis;
		ComputeTripodAxisAndVisibility(i, dirAxis, dirPlaneX, dirPlaneY, belowAxisLimit, belowPlaneLimit);

		if (_bUsing == false || (_bUsing && type == MT_MOVE_X + i))
		{
			// draw axis
			if (belowAxisLimit && Intersects(op, static_cast<OPERATION>(TRANSLATE_X << i)))
			{

				ImVec2 baseSSpace = GUI->WorldToScreenPos(dirAxis * 0.1f * _screenFactor, _mvp);
				ImVec2 worldDirSSpace = GUI->WorldToScreenPos(dirAxis * _screenFactor, _mvp);

				GAME->GetSceneDesc().drawList->AddLine(baseSSpace, worldDirSSpace, colors[i + 1], GAME->GetSceneDesc().style.TranslationLineThickness);

				// Arrow head begin
				ImVec2 dir(origin - worldDirSSpace);

				float d = sqrtf(ImLengthSqr(dir));
				dir = (dir / d) * GAME->GetSceneDesc().style.TranslationLineArrowSize;

				ImVec2 ortogonalDir(dir.y, -dir.x); // Perpendicular vector
				ImVec2 a(worldDirSSpace + dir);
				GAME->GetSceneDesc().drawList->AddTriangleFilled(worldDirSSpace - dir, a + ortogonalDir, a - ortogonalDir, colors[i + 1]);

				if (_axisFactor[i] < 0.f)				
					DrawHatchedAxis(dirAxis);
				
			}
		}
		// draw plane
		if (_bUsing == false || (_bUsing && type == MT_MOVE_YZ + i))
		{
			if (belowPlaneLimit && Contains(op, TRANSLATE_PLANS[i]))
			{
				ImVec2 screenQuadPts[4];
				for (int j = 0; j < 4; ++j)
				{
					Vec3 cornerWorldPos = (dirPlaneX * _quadUV[j * 2] + dirPlaneY * _quadUV[j * 2 + 1]) * _screenFactor;
					screenQuadPts[j] = GUI->WorldToScreenPos(cornerWorldPos, _mvp);
				}
				GAME->GetSceneDesc().drawList->AddPolyline(screenQuadPts, 4, GUI->GetColorU32(DIRECTION_X + i), true, 1.0f);
				GAME->GetSceneDesc().drawList->AddConvexPolyFilled(screenQuadPts, 4, colors[i + 4]);
			}
		}
	}

	GAME->GetSceneDesc().drawList->AddCircleFilled(_screenSquareCenter, GAME->GetSceneDesc().style.CenterCircleSize, colors[0], 32);

	if (_bUsing && IsTranslateType(type))
	{
		ImU32 translationLineColor = GUI->GetColorU32(TRANSLATION_LINE);

		ImVec2 sourcePosOnScreen = GUI->WorldToScreenPos(_matrixOrigin, _vp);
		ImVec2 destinationPosOnScreen = GUI->WorldToScreenPos(_model.Translation(), _vp);
		Vec4 dif = { destinationPosOnScreen.x - sourcePosOnScreen.x, destinationPosOnScreen.y - sourcePosOnScreen.y, 0.f, 0.f };
		dif.Normalize();
		dif *= 5.f;
		GAME->GetSceneDesc().drawList->AddCircle(sourcePosOnScreen, 6.f, translationLineColor);
		GAME->GetSceneDesc().drawList->AddCircle(destinationPosOnScreen, 6.f, translationLineColor);
		GAME->GetSceneDesc().drawList->AddLine(ImVec2(sourcePosOnScreen.x + dif.x, sourcePosOnScreen.y + dif.y), ImVec2(destinationPosOnScreen.x - dif.x, destinationPosOnScreen.y - dif.y), translationLineColor, 2.f);

		char tmps[512];
		Vec3 deltaInfo = _model.Translation() - _matrixOrigin;
		int componentInfoIndex = (type - MT_MOVE_X) * 3;

		float& f1 = ((float*)&deltaInfo.x)[s_translationInfoIndex[componentInfoIndex]];
		float& f2 = ((float*)&deltaInfo.x)[s_translationInfoIndex[componentInfoIndex + 1]];
		float& f3 = ((float*)&deltaInfo.x)[s_translationInfoIndex[componentInfoIndex + 2]];

		ImFormatString(tmps, sizeof(tmps), s_translationInfoMask[type - MT_MOVE_X], f1, f2, f3);

		GAME->GetSceneDesc().drawList->AddText(ImVec2(destinationPosOnScreen.x + 15, destinationPosOnScreen.y + 15), GUI->GetColorU32(TEXT_SHADOW), tmps);
		GAME->GetSceneDesc().drawList->AddText(ImVec2(destinationPosOnScreen.x + 14, destinationPosOnScreen.y + 14), GUI->GetColorU32(TEXT), tmps);
	}
}

void SceneWindow::DrawScaleGizmo(OPERATION op, int32 type)
{

	if (!Intersects(op, SCALE))
		return;
	
	// colors
	ImU32 colors[7];
	ComputeColors(colors, type, SCALE);

	// draw
	Vec4 scaleDisplay = { 1.f, 1.f, 1.f, 1.f };

	if (_bUsing)
	{
		scaleDisplay = Vec4(1,1,1,1);
	}

	for (int i = 0; i < 3; i++)
	{
		if (!Intersects(op, static_cast<OPERATION>(SCALE_X << i)))
		{
			continue;
		}
		const bool usingAxis = (_bUsing && type == MT_SCALE_X + i);
		if (!_bUsing || usingAxis)
		{
			Vec3 dirPlaneX, dirPlaneY, dirAxis;
			bool belowAxisLimit, belowPlaneLimit;
			ComputeTripodAxisAndVisibility(i, dirAxis, dirPlaneX, dirPlaneY, belowAxisLimit, belowPlaneLimit, true);
			belowAxisLimit = true;
			// draw axis
			if (belowAxisLimit)
			{
				bool hasTranslateOnAxis = Contains(op, static_cast<OPERATION>(TRANSLATE_X << i));
				float markerScale = hasTranslateOnAxis ? 1.4f : 1.0f;
				ImVec2 baseSSpace = GUI->WorldToScreenPos(dirAxis * 0.1f * _screenFactor, _mvp);
				ImVec2 worldDirSSpaceNoScale = GUI->WorldToScreenPos(dirAxis * markerScale * _screenFactor, _mvp);
				ImVec2 worldDirSSpace = GUI->WorldToScreenPos((dirAxis * markerScale * ((float*)&scaleDisplay)[i]) *_screenFactor, _mvp);

				if (_bUsing )
				{
					ImU32 scaleLineColor = GUI->GetColorU32(SCALE_LINE);
					GAME->GetSceneDesc().drawList->AddLine(baseSSpace, worldDirSSpaceNoScale, scaleLineColor, GAME->GetSceneDesc().style.ScaleLineThickness);
					GAME->GetSceneDesc().drawList->AddCircleFilled(worldDirSSpaceNoScale, GAME->GetSceneDesc().style.ScaleLineCircleSize, scaleLineColor);
				}

				if (!hasTranslateOnAxis || _bUsing)
				{
					GAME->GetSceneDesc().drawList->AddLine(baseSSpace, worldDirSSpace, colors[i + 1], GAME->GetSceneDesc().style.ScaleLineThickness);
				}
				GAME->GetSceneDesc().drawList->AddCircleFilled(worldDirSSpace, GAME->GetSceneDesc().style.ScaleLineCircleSize, colors[i + 1]);

				if (_axisFactor[i] < 0.f)
				{
					DrawHatchedAxis(dirAxis * ((float*)&scaleDisplay)[i]);
				}
			}
		}
	}

	// draw screen cirle
	GAME->GetSceneDesc().drawList->AddCircleFilled(_screenSquareCenter, GAME->GetSceneDesc().style.CenterCircleSize, colors[0], 32);

	if (_bUsing && IsScaleType(type))
	{

		ImVec2 destinationPosOnScreen = GUI->WorldToScreenPos(_model.Translation(),_vp);
		char tmps[512];
		int componentInfoIndex = (type - MT_SCALE_X) * 3;

		float& f1 = ((float*)&scaleDisplay.x)[s_translationInfoIndex[componentInfoIndex]];

		ImFormatString(tmps, sizeof(tmps), s_scaleInfoMask[type - MT_SCALE_X], f1);
		GAME->GetSceneDesc().drawList->AddText(ImVec2(destinationPosOnScreen.x + 15, destinationPosOnScreen.y + 15), GUI->GetColorU32(TEXT_SHADOW), tmps);
		GAME->GetSceneDesc().drawList->AddText(ImVec2(destinationPosOnScreen.x + 14, destinationPosOnScreen.y + 14), GUI->GetColorU32(TEXT), tmps);
	}
}
