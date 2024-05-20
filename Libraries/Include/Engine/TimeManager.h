#pragma once


class TimeManager
{
	DECLARE_SINGLE(TimeManager);

public:
	void Init();
	void Update();
	void Reset();

	uint32 GetFps() { return _fps; }
	float GetDeltaTime() { return _deltaTime; }

	float GetTotalTime() const
	{
		return static_cast<float>((_currTime - _baseTime) / _frequency);
	}

private:
	uint64	_frequency = 0;
	uint64	_prevCount = 0;
	float	_deltaTime = 0.f;

private:
	uint32	_frameCount = 0;
	float	_frameTime = 0.f;
	uint32	_fps = 0;

	uint64 _baseTime = 0;
	uint64 _currTime = 0;
};

