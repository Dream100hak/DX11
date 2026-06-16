#pragma once
#include "MonoBehaviour.h"
#include "Transform.h"
#include "Define.h"
#include "TimeManager.h"
#include "imgui.h"

// 예제 스크립트 — MonoBehaviour 시연 (Play 모드에서 동작).
// DX11 엔진의 게임 로직 스크립트(MonoBehaviour 파생)에 대응.

// 트랜스폼 회전
class Rotator : public MonoBehaviour
{
public:
	float _speed = 90.f; int _axis = 1; // deg/s, 0=X 1=Y 2=Z
	const char* TypeName() const override { return "Rotator"; }
	void Update() override
	{
		auto t = GetTransform(); if (!t) return;
		Vec3 r = t->GetLocalRotation();
		float d = _speed * DT * 0.01745329f;
		if (_axis == 0) r.x += d; else if (_axis == 1) r.y += d; else r.z += d;
		t->SetLocalRotation(r);
	}
	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Rotator (Script)");
		const char* ax[] = { "X", "Y", "Z" }; ImGui::Combo("Axis", &_axis, ax, 3);
		ImGui::DragFloat("Speed deg/s", &_speed, 1.f);
	}
	void Serialize(std::ostream& o) override { o << _axis << ' ' << _speed; }
	void Deserialize(std::istream& i) override { i >> _axis >> _speed; }
};

// Y 사인 진동
class Bobber : public MonoBehaviour
{
public:
	float _amp = 0.5f, _freq = 1.f;
	const char* TypeName() const override { return "Bobber"; }
	void Update() override
	{
		auto t = GetTransform(); if (!t) return;
		Vec3 p = t->GetLocalPosition();
		if (!_init) { _baseY = p.y; _init = true; }
		_t += DT;
		p.y = _baseY + sinf(_t * _freq * 6.2831853f) * _amp;
		t->SetLocalPosition(p);
	}
	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Bobber (Script)");
		ImGui::DragFloat("Amplitude", &_amp, 0.01f);
		ImGui::DragFloat("Frequency", &_freq, 0.01f);
	}
	void Serialize(std::ostream& o) override { o << _amp << ' ' << _freq; }
	void Deserialize(std::istream& i) override { i >> _amp >> _freq; }
private:
	float _t = 0.f, _baseY = 0.f; bool _init = false;
};

inline void RegisterBuiltinScripts()
{
	ScriptRegistry::Register("Rotator", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<Rotator>()); });
	ScriptRegistry::Register("Bobber", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<Bobber>()); });
}
