#pragma once

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


enum OPERATION
{
	TRANSLATE_X = (1u << 0),
	TRANSLATE_Y = (1u << 1),
	TRANSLATE_Z = (1u << 2),
	ROTATE_X = (1u << 3),
	ROTATE_Y = (1u << 4),
	ROTATE_Z = (1u << 5),
	ROTATE_SCREEN = (1u << 6),
	SCALE_X = (1u << 7),
	SCALE_Y = (1u << 8),
	SCALE_Z = (1u << 9),
	BOUNDS = (1u << 10),
	SCALE_XU = (1u << 11),
	SCALE_YU = (1u << 12),
	SCALE_ZU = (1u << 13),

	TRANSLATE = TRANSLATE_X | TRANSLATE_Y | TRANSLATE_Z,
	ROTATE = ROTATE_X | ROTATE_Y | ROTATE_Z | ROTATE_SCREEN,
	SCALE = SCALE_X | SCALE_Y | SCALE_Z,
	SCALEU = SCALE_XU | SCALE_YU | SCALE_ZU, // universal
	UNIVERSAL = TRANSLATE | ROTATE | SCALEU
};

enum COLOR
{
	DIRECTION_X,      // directionColor[0]
	DIRECTION_Y,      // directionColor[1]
	DIRECTION_Z,      // directionColor[2]
	PLANE_X,          // planeColor[0]
	PLANE_Y,          // planeColor[1]
	PLANE_Z,          // planeColor[2]
	SELECTION,        // selectionColor
	INACTIVE,         // inactiveColor
	TRANSLATION_LINE, // translationLineColor
	SCALE_LINE,
	ROTATION_USING_BORDER,
	ROTATION_USING_FILL,
	HATCHED_AXIS_LINES,
	TEXT,
	TEXT_SHADOW,
	COUNT
};

struct Style
{
	IMGUI_API Style()
	{
		// default values
		TranslationLineThickness = 3.0f;
		TranslationLineArrowSize = 6.0f;
		RotationLineThickness = 2.0f;
		RotationOuterLineThickness = 3.0f;
		ScaleLineThickness = 3.0f;
		ScaleLineCircleSize = 6.0f;
		HatchedAxisLineThickness = 6.0f;
		CenterCircleSize = 6.0f;

		// initialize default colors
		Colors[DIRECTION_X] = ImVec4(0.666f, 0.000f, 0.000f, 1.000f);
		Colors[DIRECTION_Y] = ImVec4(0.000f, 0.666f, 0.000f, 1.000f);
		Colors[DIRECTION_Z] = ImVec4(0.000f, 0.000f, 0.666f, 1.000f);
		Colors[PLANE_X] = ImVec4(0.666f, 0.000f, 0.000f, 0.380f);
		Colors[PLANE_Y] = ImVec4(0.000f, 0.666f, 0.000f, 0.380f);
		Colors[PLANE_Z] = ImVec4(0.000f, 0.000f, 0.666f, 0.380f);
		Colors[SELECTION] = ImVec4(1.000f, 0.500f, 0.062f, 0.541f);
		Colors[INACTIVE] = ImVec4(0.600f, 0.600f, 0.600f, 0.600f);
		Colors[TRANSLATION_LINE] = ImVec4(0.666f, 0.666f, 0.666f, 0.666f);
		Colors[SCALE_LINE] = ImVec4(0.250f, 0.250f, 0.250f, 1.000f);
		Colors[ROTATION_USING_BORDER] = ImVec4(1.000f, 0.500f, 0.062f, 1.000f);
		Colors[ROTATION_USING_FILL] = ImVec4(1.000f, 0.500f, 0.062f, 0.500f);
		Colors[HATCHED_AXIS_LINES] = ImVec4(0.000f, 0.000f, 0.000f, 0.500f);
		Colors[TEXT] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
		Colors[TEXT_SHADOW] = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
	}

	float TranslationLineThickness;   // Thickness of lines for translation gizmo
	float TranslationLineArrowSize;   // Size of arrow at the end of lines for translation gizmo
	float RotationLineThickness;      // Thickness of lines for rotation gizmo
	float RotationOuterLineThickness; // Thickness of line surrounding the rotation gizmo
	float ScaleLineThickness;         // Thickness of lines for scale gizmo
	float ScaleLineCircleSize;        // Size of circle at the end of lines for scale gizmo
	float HatchedAxisLineThickness;   // Thickness of hatched axis lines
	float CenterCircleSize;           // Size of circle at the center of the translate/scale gizmo

	ImVec4 Colors[COLOR::COUNT];
};


struct SceneDesc
{
	ImDrawList* drawList;
	Style style;

	float x = 0.f;
	float y = 21.f;
	float width = 800.f;
	float height = 530.f;
};


struct GameDesc
{
	shared_ptr<class IExecute> app = nullptr;
	wstring appName = L"GameCoding";
	HINSTANCE hInstance = 0;
	HWND hWnd = 0;
	float width = 1600;
	float height = 900;
	bool vsync = false;
	bool windowed = true;
	
	float sceneWidth = 800;
	float sceneHeight = 600;

	Color clearColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
};

class Game
{
	DECLARE_SINGLE(Game);
public:
	
	WPARAM Run(GameDesc& gameDesc, SceneDesc& sceneDesc);
	LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);

	GameDesc& GetGameDesc() { return _gameDesc; }
	SceneDesc& GetSceneDesc() { return _sceneDesc; }

private:
	ATOM MyRegisterClass();
	BOOL InitInstance(int cmdShow);

	void Update();
	void ShowFps();

private:
	GameDesc _gameDesc;
	SceneDesc _sceneDesc;


};

