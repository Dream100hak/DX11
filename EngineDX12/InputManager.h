#pragma once
#include "Common.h"
#include "Define.h"

// DX11 Engine/InputManager 이식(1차) — 키/마우스 상태 스냅샷. INPUT 매크로로 접근.
// (FlyCamera 는 아직 GetAsyncKeyState 직접 사용 — 추후 이 매니저로 일원화)
class InputManager
{
	DECLARE_SINGLE(InputManager);

public:
	void Update()
	{
		for (int i = 0; i < 256; ++i)
		{
			_prev[i] = _cur[i];
			_cur[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
		}
	}
	bool GetButton(int vkey)     const { return _cur[vkey]; }
	bool GetButtonDown(int vkey) const { return _cur[vkey] && !_prev[vkey]; }
	bool GetButtonUp(int vkey)   const { return !_cur[vkey] && _prev[vkey]; }

private:
	bool _cur[256]{};
	bool _prev[256]{};
};
