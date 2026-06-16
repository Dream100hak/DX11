#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// FlyCamera — 자유 비행 카메라.
// 우클릭 드래그 마우스 룩 + WASD 이동 + Q/E 상하 + Shift 가속, F 포커스, W/E/R 기즈모 단축키.
// 입력 게이팅(Scene 뷰 hover/focus)·포커스 대상 AABB·기즈모 모드 출력은 InputCtx 로 호출측(에디터)이 전달.
// 상태(pos/yaw/pitch/fov/...)는 public — 인스펙터/씬 저장이 직접 편집한다.
// ───────────────────────────────────────────────────────────
class FlyCamera
{
public:
	struct InputCtx
	{
		HWND  hwnd;
		bool  inputAllowed;   // 마우스 룩 허용 (Scene hover 또는 에디터 미준비)
		bool  keysAllowed;    // 단축키/포커스 허용 (Scene focus 또는 에디터 미준비)
		DirectX::XMFLOAT3 focusMin, focusMax; // F 포커스 대상 AABB
		int*  gizmoOp;        // W/E/R → 기즈모 모드 출력 (null 이면 무시)
	};

	void Update(float dt, const InputCtx& in); // 입력 → pos/yaw/pitch 갱신
	void Orbit(float t);                        // 자동 오빗 (원점 중심) — pos/yaw/pitch 덮어씀

	DirectX::XMMATRIX View() const;
	DirectX::XMMATRIX Proj(float aspect) const;
	DirectX::XMVECTOR Eye() const { return DirectX::XMLoadFloat3(&pos); }
	DirectX::XMVECTOR Forward() const;
	bool              Flying() const { return _rmbDown; }

	// ── 상태 (인스펙터/씬 저장이 직접 접근) ──
	DirectX::XMFLOAT3 pos{ 3.4f, 2.4f, -4.6f };
	float yaw = -0.637f;   // 초기 시선(원점 부근)
	float pitch = -0.232f;
	float fov = 55.0f, moveSpeed = 3.5f, fastMul = 2.6f;
	float nearZ = 0.1f, farZ = 200.0f;
	bool  orbit = false;
	struct Bookmark { DirectX::XMFLOAT3 pos; float yaw, pitch; bool set = false; };
	Bookmark bm[4];

private:
	bool  _rmbDown = false;
	POINT _lastCursor{ 0, 0 };
};
