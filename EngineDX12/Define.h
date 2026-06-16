#pragma once
#include "Common.h"
#include <cassert>

// ───────────────────────────────────────────────────────────
// 싱글톤 / 공용 매크로 — DX11 Engine/Define.h 이식 (DX12 포팅 토대).
// 매니저(Graphics/Scene/Input/Time/GUI 등) 싱글톤은 포팅 진행하며 매크로 추가.
// ───────────────────────────────────────────────────────────
#define DECLARE_SINGLE(classname)               \
private:                                        \
	classname() {}                              \
public:                                         \
	static classname* GetInstance()             \
	{                                           \
		static classname s_instance;            \
		return &s_instance;                     \
	}

#define GET_SINGLE(classname)  classname::GetInstance()
#define CHECK(p)               assert(SUCCEEDED(p))

// 매니저 싱글톤 접근 (포팅 진행하며 GRAPHICS/INPUT/TIME/GUI 등 추가)
#define SCENE      GET_SINGLE(SceneManager)
#define CUR_SCENE  SCENE->GetCurrentScene()
#define MAIN_CAM   CUR_SCENE->GetMainCamera()->GetCamera()
#define TIME       GET_SINGLE(TimeManager)
#define DT         TIME->GetDeltaTime()
#define INPUT      GET_SINGLE(InputManager)
#define RESOURCES  GET_SINGLE(ResourceManager)
#define GUI        GET_SINGLE(ImGuiManager)
// DX12판 그래픽스 접근 (D3D12Device 가 사실상 Graphics — 정적 접근자로 노출)
#define GRAPHICS   D3D12Device::Get()
#define DEVICE     (GRAPHICS->Device())
#define CMDLIST    (GRAPHICS->Cmd())

#define MAX_LIGHTS            16
#define CASCADE_COUNT        4
#define MAX_PUNCTUAL_SHADOWS 4

enum LayerMask
{
	Default = 0,
	UI = 1,
	Wall = 2,
	Invisible = 3,
};

// ── 렌더 큐 (Engine 과 동일 값) ──
enum class RenderQueue : int32
{
	Background  = 1000,
	Opaque      = 2000,
	AlphaTest   = 2450,
	Transparent = 3000,
	Overlay     = 4000,
};
