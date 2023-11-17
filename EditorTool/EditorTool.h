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



class EditorTool : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

	void OnMouseWheel(int32 scrollAmount) override;

	void DrawTranslationGizmo(int32 type,  OPERATION op);

	bool Intersects(OPERATION lhs, OPERATION rhs)
	{
		return (lhs & rhs) != 0;
	}
	void  ComputeTripodAxisAndVisibility(const Matrix& mvp, const int axisIndex, Vec3& dirAxis, Vec3& dirPlaneX, Vec3& dirPlaneY, bool& belowAxisLimit, bool& belowPlaneLimit, const bool localCoordinates = false);
	float GetSegmentLengthClipSpace(const Vec3& start, const Vec3& end);
	float GetParallelogram(const Matrix& mvp , const Vec3& ptO, const Vec3& ptA, const Vec3& ptB);
	void  DrawHatchedAxis(const Vec3& axis);

	bool  Manipulate(OPERATION operation);
	bool  IsTranslateType(int type);
	float IntersectRayPlane(const Vec4& rOrigin, const Vec4& rVector, const Vec4& plan);
	bool  HandleTranslation(OPERATION op, int& type, const float* snap);

	void ComputeContext(shared_ptr<Transform> obj);

	void ComputeColors(ImU32* colors, int type, OPERATION operation)
	{
		ImU32 selectionColor = GUI->GetColorU32(SELECTION);

		switch (operation)
		{
		case TRANSLATE:
			colors[0] = (type == MT_MOVE_SCREEN) ? IM_COL32_WHITE : IM_COL32_WHITE;
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

	bool Contains(OPERATION lhs, OPERATION rhs)
	{
		return (lhs & rhs) == rhs;
	}

private:

	bool _showWindow = true;
	shared_ptr<class SceneCamera> _sceneCam;
	shared_ptr<class Button> _btn; 

	Matrix _view;
	Matrix _projection;
	Matrix _model;
	Matrix _modelLocal; // orthonormalized model
	Matrix _modelInverse;
	Matrix _modelSource;
	Matrix _modelSourceInverse;
	Matrix _mvp;
	Matrix _mvpLocal; // MVP with full model matrix whereas mMVP's model matrix might only be translation in case of World space edition
	Matrix _vp;

	

private:
	float _quadMin = 0.5f;
	float _quadMax = 0.8f;
	float _quadUV[8] = { _quadMin, _quadMin, _quadMin, _quadMax, _quadMax, _quadMax, _quadMax, _quadMin };
	
};
static inline ImVec2 operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline ImVec2 operator/(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }