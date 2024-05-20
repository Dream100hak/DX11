#include "pch.h"
#include "TimeManager.h"

void TimeManager::Init()
{
	LARGE_INTEGER frequency;
	::QueryPerformanceFrequency(&frequency);
	_frequency = static_cast<double>(frequency.QuadPart);

	LARGE_INTEGER prevCount;
	::QueryPerformanceCounter(&prevCount);
	_prevCount = prevCount.QuadPart;

	_baseTime = _prevCount;
}

void TimeManager::Update()
{
	LARGE_INTEGER currentCount;
	::QueryPerformanceCounter(&currentCount);
	_currTime = currentCount.QuadPart;

	_deltaTime = (_currTime - _prevCount) / static_cast<float>(_frequency);
	_prevCount = _currTime;

	_frameCount++;
	_frameTime += _deltaTime;

	if (_frameTime > 1.f)
	{
		_fps = static_cast<uint32>(_frameCount / _frameTime);

		_frameTime = 0.f;
		_frameCount = 0;
	}
}

void TimeManager::Reset()
{
	LARGE_INTEGER prevCount;
	::QueryPerformanceCounter(&prevCount);
	_prevCount = prevCount.QuadPart;

	_baseTime = _prevCount;
}
