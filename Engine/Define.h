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
#define CUR_SCENE	SCENE->GetCurrentScene()
#define MAIN_CAM	SCENE->GetCurrentScene()->GetMainCamera()->GetCamera()

#define MODEL_GLOBAL_SCALE 	2.07744789f;

#define JOB_PRE_RENDER	GRAPHICS->GetPreRenderJobQueue()
#define JOB_RENDER		GRAPHICS->GetRenderJobQueue()
#define JOB_POST_RENDER	GRAPHICS->GetPostRenderJobQueue()


#define TECH_TEXTURE	0;
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

