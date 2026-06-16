#pragma once
#include "Common.h"
#include "Define.h"

// DX11 Engine/TimeManager 이식 — 실측 델타타임/FPS. TIME / DT 매크로로 접근.
class TimeManager
{
	DECLARE_SINGLE(TimeManager);

public:
	void Init()
	{
		QueryPerformanceFrequency(&_freq);
		QueryPerformanceCounter(&_prev);
	}
	void Update()
	{
		LARGE_INTEGER cur; QueryPerformanceCounter(&cur);
		_deltaTime = float(double(cur.QuadPart - _prev.QuadPart) / double(_freq.QuadPart));
		if (_deltaTime > 0.1f) _deltaTime = 0.1f; // 스파이크 클램프 (브레이크포인트 등)
		_prev = cur;
		_accum += _deltaTime; _frames++;
		if (_accum >= 1.0f) { _fps = _frames / _accum; _accum = 0.f; _frames = 0; }
	}
	float GetDeltaTime() const { return _deltaTime; }
	float GetFps() const { return _fps; }

private:
	LARGE_INTEGER _freq{}, _prev{};
	float _deltaTime = 1.f / 60.f;
	float _accum = 0.f, _fps = 0.f;
	int   _frames = 0;
};
