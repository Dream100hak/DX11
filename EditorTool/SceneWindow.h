#pragma once
#include "EditorWindow.h"



enum MOVETYPE
{
	MT_NONE,
	MT_MOVE_X,
	MT_MOVE_Y,
	MT_MOVE_Z,
	MT_MOVE_YZ,
	MT_MOVE_ZX,
	MT_MOVE_XY,
	MT_MOVE_SCREEN,
	MT_ROTATE_X,
	MT_ROTATE_Y,
	MT_ROTATE_Z,
	MT_ROTATE_SCREEN,
	MT_SCALE_X,
	MT_SCALE_Y,
	MT_SCALE_Z,
	MT_SCALE_XYZ
};


inline OPERATION operator|(OPERATION lhs, OPERATION rhs)
{
	return static_cast<OPERATION>(static_cast<int>(lhs) | static_cast<int>(rhs));
}


class SceneWindow : public EditorWindow
{

	enum Mode
	{
		Local,
		World,
	};

public:
	SceneWindow();
	~SceneWindow();

	virtual void Init() override;
	virtual void Update() override;

	void ShowSceneWindow();

	bool Intersects(OPERATION lhs, OPERATION rhs) { return (lhs & rhs) != 0; }

	bool IsTranslateType(int32 type) { return type >= MT_MOVE_X && type <= MT_MOVE_SCREEN;  }
	bool IsScaleType(int32 type) { return type >= MT_SCALE_X && type <= MT_SCALE_XYZ; }

	float IntersectRayPlane(const XMVECTOR& rOrigin, const XMVECTOR& rVector, const Vec4& plane);
	float GetSegmentLengthClipSpace(const Vec3& start, const Vec3& end, const bool localCoordinates = false);
	
	Plane BuildPlan(const Vec3& pointOrigin, Vec3& normalOrigin);

	void EditTransform();
	bool Manipulate(OPERATION operation, Mode mode, const float* snap, const float* localBounds, const float* boundsSnap);
	
	bool HandleTranslation(OPERATION op, int& type, Mode mode, const float* snap);
	bool HandleScale(OPERATION op, int& type, Mode mode, const float* snap);

	int32 GetMoveType(OPERATION op, Vec3& gizmoHitProportion);
	int32 GetScaleType(OPERATION op);

	void DrawTranslationGizmo(OPERATION op, int32 type);
	void DrawScaleGizmo(OPERATION op, int32 type);

	void ComputeContext(Mode mode);
	void ComputeColors(ImU32* colors, int type, OPERATION operation)
	{
		ImU32 selectionColor = GUI->GetColorU32(SELECTION);

		switch (operation)
		{
		case TRANSLATE:
			colors[0] = (type == MT_MOVE_SCREEN) ? selectionColor : IM_COL32_WHITE;
			for (int i = 0; i < 3; i++)
			{
				colors[i + 1] = (type == (int)(MT_MOVE_X + i)) ? selectionColor : GUI->GetColorU32(DIRECTION_X + i);
				colors[i + 4] = (type == (int)(MT_MOVE_YZ + i)) ? selectionColor : GUI->GetColorU32(PLANE_X + i);
				colors[i + 4] = (type == MT_MOVE_SCREEN) ? selectionColor : colors[i + 4];
			}
			break;
		case ROTATE:
			colors[0] = (type == MT_ROTATE_SCREEN) ? selectionColor : IM_COL32_WHITE;
			for (int i = 0; i < 3; i++)
			{
				colors[i + 1] = (type == (int)(MT_ROTATE_X + i)) ? selectionColor : GUI->GetColorU32(DIRECTION_X + i);
			}
			break;
		case SCALEU:
		case SCALE:
			colors[0] = (type == MT_SCALE_XYZ) ? selectionColor : IM_COL32_WHITE;
			for (int i = 0; i < 3; i++)
			{
				colors[i + 1] = (type == (int)(MT_SCALE_X + i)) ? selectionColor : GUI->GetColorU32(DIRECTION_X + i);
			}
			break;
			// note: this internal function is only called with three possible values for operation
		default:
			break;
		}
	}
	void ComputeCameraRay(Vec4& rayOrigin, Vec4& rayDir);

	bool Contains(OPERATION lhs, OPERATION rhs) { return (lhs & rhs) == rhs; }
	void DrawHatchedAxis(const Vec3& axis);

	ImVec2 PointOnSegment(const ImVec2& point, const ImVec2& vertPos1, const ImVec2& vertPos2);
	float GetParallelogram(const Vec3& ptO, const Vec3& ptA, const Vec3& ptB);
	void ComputeTripodAxisAndVisibility(const int axisIndex, Vec3& dirAxis, Vec3& dirPlaneX, Vec3& dirPlaneY, bool& belowAxisLimit, bool& belowPlaneLimit, const bool localCoordinates = false);
	
	float Dot3(XMVECTOR v1, XMVECTOR v2) {
		XMVECTOR dot = XMVector3Dot(v1, v2); // 3D 점곱 계산
		return XMVectorGetX(dot); // dot의 x 성분 반환
	}

private:

	
	Matrix _view;
	Matrix _projection;

	Matrix _model;
	Matrix _modelLocal; // orthonormalized model
	Matrix _modelInverse;
	Matrix _modelSource;
	Matrix _modelSourceInverse;

	Matrix _mvp;
	Matrix _vp;

	Vec3 _modelScaleOrigin;
	Ray _ray; 

	Vec4 _rayOrigin;
	Vec4 _rayDir;

	Plane _translationPlan;
	Vec4 _translationPlanOrigin;
	Vec3 _translationLastDelta;

	Vec3 _matrixOrigin;
	Vec3 _relativeOrigin;

	Vec4 _scale;
	Vec3 _scaleValueOrigin;
	Vec3 _scaleLastDelta;
	float _saveMousePosX;

	Vec3 _cameraDir;
	Vec3 _cameraEye;
	Vec3 _cameraRight;
	Vec3 _cameraUp;

	shared_ptr<Transform> _tr;	
	int32 _currentOperation = -1;
	OPERATION _currentGizmoOperation = TRANSLATE;

	const OPERATION TRANSLATE_PLANS[3] = { TRANSLATE_Y | TRANSLATE_Z, TRANSLATE_X | TRANSLATE_Z, TRANSLATE_X | TRANSLATE_Y };
	static const int s_translationInfoIndex[];

	static const char* s_translationInfoMask[];
	static const char* s_scaleInfoMask[];

private:
	float _screenFactor = 500.f;
	ImVec2 _screenSquareCenter ;
	ImVec2 _screenSquareMin ;
	ImVec2 _screenSquareMax ;

	float _gizmoSizeClipSpace = 0.2f;
	float _displayRatio = 1.f;

	bool _belowAxisLimit[3];
	bool _belowPlaneLimit[3];
	float _axisFactor[3];

	float _quadMin = 0.2f;
	float _quadMax = 0.5f;
	float _quadUV[8] = { _quadMin, _quadMin, _quadMin, _quadMax, _quadMax, _quadMax, _quadMax, _quadMin };

	bool _reserved = false;
	bool _allowFlip = false;

	float _axisLimit = 0.0025f;
	float _planeLimit = 0.02f;

	bool _bUsing = false;

};

static inline ImVec2 operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline ImVec2 operator/(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
