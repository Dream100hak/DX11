#pragma once

#define DECLARE_SINGLE(classname)			\
private:									\
	classname() { }							\
public:										\
	static classname* GetInstance()			\
	{										\
		static classname s_instance;		\
		return &s_instance;					\
	}

#define GET_SINGLE(classname)	classname::GetInstance()

#define CHECK(p)	assert(SUCCEEDED(p))
#define GAME		GET_SINGLE(Game)		

#define GRAPHICS	GET_SINGLE(Graphics)
#define DEVICE		GRAPHICS->GetDevice()
#define DCT			GRAPHICS->GetDeviceContext()

#define INPUT		GET_SINGLE(InputManager)
#define TIME		GET_SINGLE(TimeManager)
#define DT			TIME->GetDeltaTime()
#define RESOURCES	GET_SINGLE(ResourceManager)
#define INSTANCING	GET_SINGLE(InstancingManager)
#define GUI			GET_SINGLE(ImGuiManager)
#define SCENE		GET_SINGLE(SceneManager)
#define PROJECT		GET_SINGLE(ProjectManager)
#define RENDER_STATES GET_SINGLE(RenderStateManager)

#define CUR_SCENE	SCENE->GetCurrentScene()
#define MAIN_CAM	SCENE->GetCurrentScene()->GetMainCamera()->GetCamera()

#define MODEL_GLOBAL_SCALE 	2.07744789f;

// ──────────────────────────────────────────────────────────
// 멀티 라이트 지원
// ──────────────────────────────────────────────────────────
#define MAX_LIGHTS	16

#define TECH_NORMAL		0;
#define TECH_OUTLINE	1;
#define TECH_COLOR		2;
#define TECH_WIREFRAME	3;
#define TECH_CLOCKWISE	4;


enum LayerMask
{
	Default = 0,
	UI = 1,
	Wall = 2,
	Invisible = 3
};

// ── 렌더 큐 ────────────────────────────────────────────────
// Background  : 스카이박스 (뒤에서 먼저)
// Opaque      : 불투명 오브젝트  → Front-to-Back 정렬 (Early-Z 활용)
// AlphaTest   : 알파 클립 오브젝트 (나뭇잎 등)
// Transparent : 반투명 오브젝트  → Back-to-Front 정렬 (알파 블렌딩)
// Overlay     : UI / 이펙트 (가장 나중)
enum class RenderQueue : int32
{
	Background  = 1000,
	Opaque      = 2000,
	AlphaTest   = 2450,
	Transparent = 3000,
	Overlay     = 4000,
};

