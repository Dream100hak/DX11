#include "FlyCamera.h"

using namespace DirectX;

// 카메라 시선 벡터 (yaw=Y축, pitch=X축, LH: yaw=0 → +Z)
static XMVECTOR CamForward(float yaw, float pitch)
{
	float cp = cosf(pitch);
	return XMVectorSet(cp * sinf(yaw), sinf(pitch), cp * cosf(yaw), 0.f);
}

XMVECTOR FlyCamera::Forward() const { return CamForward(yaw, pitch); }

XMMATRIX FlyCamera::View() const
{
	XMVECTOR eye = XMLoadFloat3(&pos);
	return XMMatrixLookAtLH(eye, XMVectorAdd(eye, CamForward(yaw, pitch)), XMVectorSet(0, 1, 0, 0));
}

XMMATRIX FlyCamera::Proj(float aspect) const
{
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect, nearZ, farZ);
}

void FlyCamera::Orbit(float t)
{
	float ang = t * 0.3f;
	pos = { cosf(ang) * 6.0f, 3.0f, sinf(ang) * 6.0f };
	XMVECTOR d = XMVector3Normalize(XMVectorSubtract(XMVectorSet(0, 1.0f, 0, 0), XMLoadFloat3(&pos)));
	XMFLOAT3 df; XMStoreFloat3(&df, d);
	yaw = atan2f(df.x, df.z); pitch = asinf(df.y);
}

void FlyCamera::Update(float dt, const InputCtx& in)
{
	// 마우스 룩 (우클릭 유지 동안 — 윈도우 중앙 재고정 방식으로 델타 측정)
	bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	bool focused = (GetForegroundWindow() == in.hwnd);
	if (rmb && focused && (in.inputAllowed || _rmbDown))
	{
		POINT cur; GetCursorPos(&cur);
		if (!_rmbDown) { _lastCursor = cur; _rmbDown = true; ShowCursor(FALSE); }
		float dx = float(cur.x - _lastCursor.x), dy = float(cur.y - _lastCursor.y);
		const float sens = 0.0032f;
		yaw   += dx * sens;
		pitch -= dy * sens;
		pitch = max(-1.553f, min(1.553f, pitch)); // ±89°
		// 커서를 윈도우 중앙으로 재고정 (무한 회전)
		RECT rc; GetClientRect(in.hwnd, &rc);
		POINT c{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
		ClientToScreen(in.hwnd, &c);
		SetCursorPos(c.x, c.y);
		_lastCursor = c;
	}
	else if (_rmbDown) { _rmbDown = false; ShowCursor(TRUE); }

	if (!focused) return;

	XMVECTOR fwd = CamForward(yaw, pitch);
	XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, fwd));

	// 기즈모 단축키 + F 포커스 (플라이 아닐 때, Scene 포커스 시)
	if (in.keysAllowed && !_rmbDown)
	{
		if (in.gizmoOp)
		{
			if (GetAsyncKeyState('W') & 0x8000) *in.gizmoOp = 7;   // translate
			if (GetAsyncKeyState('E') & 0x8000) *in.gizmoOp = 120; // rotate
			if (GetAsyncKeyState('R') & 0x8000) *in.gizmoOp = 896; // scale
		}
		if (GetAsyncKeyState('F') & 0x8000)
		{
			XMVECTOR c = XMVectorScale(XMVectorAdd(XMLoadFloat3(&in.focusMin), XMLoadFloat3(&in.focusMax)), 0.5f);
			XMVECTOR ext = XMVectorSubtract(XMLoadFloat3(&in.focusMax), XMLoadFloat3(&in.focusMin));
			float rad = XMVectorGetX(XMVector3Length(ext)) * 0.5f + 0.5f;
			XMStoreFloat3(&pos, XMVectorSubtract(c, XMVectorScale(fwd, rad * 2.6f)));
		}
		return; // 플라이 아니면 이동 안 함 (단축키 우선)
	}

	if (!_rmbDown) return; // 이동은 우클릭 플라이 모드에서만

	float speed = moveSpeed * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? fastMul : 1.0f) * dt;
	XMVECTOR p = XMLoadFloat3(&pos);
	if (GetAsyncKeyState('W') & 0x8000) p = XMVectorAdd(p, XMVectorScale(fwd, speed));
	if (GetAsyncKeyState('S') & 0x8000) p = XMVectorSubtract(p, XMVectorScale(fwd, speed));
	if (GetAsyncKeyState('D') & 0x8000) p = XMVectorAdd(p, XMVectorScale(right, speed));
	if (GetAsyncKeyState('A') & 0x8000) p = XMVectorSubtract(p, XMVectorScale(right, speed));
	if (GetAsyncKeyState('E') & 0x8000) p = XMVectorAdd(p, XMVectorScale(up, speed));
	if (GetAsyncKeyState('Q') & 0x8000) p = XMVectorSubtract(p, XMVectorScale(up, speed));
	XMStoreFloat3(&pos, p);
}
