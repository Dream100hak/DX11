#pragma once
#include "MonoBehaviour.h"
#include "Transform.h"
#include "Define.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "GameObject.h"
#include "ModelAnimator.h"
#include "SceneManager.h"
#include "Scene.h"
#include "imgui.h"
#include <cmath>

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

// ── 3인칭 액션: 플레이어 컨트롤러 (WASD 이동 + Shift 달리기 + Animator Speed 구동) ──
// 월드 기준 이동(W=+Z), 이동방향으로 부드럽게 회전. 형제 ModelAnimator 의 Speed 파라미터로
// 상태머신(Idle↔Run) 구동 — SetupLocomotion() 으로 미리 세팅된 애니메이터 전제.
class PlayerController : public MonoBehaviour
{
public:
	float _speed = 3.5f, _runMul = 1.9f, _turn = 12.f;
	const char* TypeName() const override { return "PlayerController"; }
	void Update() override
	{
		auto t = GetTransform(); if (!t) return;
		auto in = GET_SINGLE(InputManager);
		float x = 0, z = 0;
		if (in->GetButton('W')) z += 1.f; if (in->GetButton('S')) z -= 1.f;
		if (in->GetButton('D')) x += 1.f; if (in->GetButton('A')) x -= 1.f;
		float len = sqrtf(x * x + z * z);
		auto go = GetGameObject();
		auto an = go ? go->GetModelAnimator() : nullptr;
		if (len > 0.01f)
		{
			x /= len; z /= len;
			bool run = in->GetButton(VK_SHIFT);
			float spd = _speed * (run ? _runMul : 1.f);
			Vec3 p = t->GetLocalPosition();
			p.x += x * spd * DT; p.z += z * spd * DT;
			t->SetLocalPosition(p);
			// 이동방향으로 yaw 회전 (최단경로 보간)
			float targetYaw = atan2f(x, z);
			Vec3 r = t->GetLocalRotation();
			float dy = targetYaw - r.y;
			while (dy > 3.14159265f) dy -= 6.2831853f;
			while (dy < -3.14159265f) dy += 6.2831853f;
			r.y += dy * (_turn * DT > 1.f ? 1.f : _turn * DT);
			t->SetLocalRotation(r);
			if (an) an->SetFloat("Speed", run ? 2.f : 1.f);
		}
		else if (an) an->SetFloat("Speed", 0.f);
	}
	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Player Controller (Script)");
		ImGui::DragFloat("Move Speed", &_speed, 0.05f, 0.f, 20.f);
		ImGui::DragFloat("Run Mul", &_runMul, 0.02f, 1.f, 4.f);
		ImGui::DragFloat("Turn Speed", &_turn, 0.2f, 0.f, 40.f);
		ImGui::TextDisabled("Play 모드: WASD 이동 / Shift 달리기");
	}
	void Serialize(std::ostream& o) override { o << _speed << ' ' << _runMul << ' ' << _turn; }
	void Deserialize(std::istream& i) override { i >> _speed >> _runMul >> _turn; }
};

// ── 3인칭 추적 카메라 (타겟 뒤 고정각, 위치 추적 + 룩앳) ──
class FollowCamera : public MonoBehaviour
{
public:
	float _dist = 6.f, _height = 3.2f, _lookH = 1.2f, _lerp = 8.f;
	const char* TypeName() const override { return "FollowCamera"; }
	void SetTarget(shared_ptr<GameObject> go) { _target = go; }
	void Update() override
	{
		auto self = GetTransform(); if (!self) return;
		auto tgt = _target.lock(); if (!tgt) return;
		auto tt = tgt->GetTransform(); if (!tt) return;
		Vec3 tp = tt->GetLocalPosition();
		Vec3 want{ tp.x, tp.y + _height, tp.z - _dist }; // 월드 -Z(뒤) 고정각
		Vec3 cur = self->GetLocalPosition();
		float a = _lerp * DT > 1.f ? 1.f : _lerp * DT;
		Vec3 np{ cur.x + (want.x - cur.x) * a, cur.y + (want.y - cur.y) * a, cur.z + (want.z - cur.z) * a };
		self->SetLocalPosition(np);
		// 타겟 룩앳 → Euler(pitch,yaw) (RotationRollPitchYaw 규약)
		Vec3 d{ tp.x - np.x, (tp.y + _lookH) - np.y, tp.z - np.z };
		float horiz = sqrtf(d.x * d.x + d.z * d.z);
		float yaw = atan2f(d.x, d.z);
		float pitch = -atan2f(d.y, horiz);
		self->SetLocalRotation(Vec3{ pitch, yaw, 0.f });
	}
	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Follow Camera (Script)");
		ImGui::DragFloat("Distance", &_dist, 0.1f, 1.f, 30.f);
		ImGui::DragFloat("Height", &_height, 0.1f, 0.f, 20.f);
		ImGui::DragFloat("Smooth", &_lerp, 0.2f, 0.5f, 30.f);
	}
private:
	weak_ptr<GameObject> _target;
};

inline void RegisterBuiltinScripts()
{
	ScriptRegistry::Register("Rotator", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<Rotator>()); });
	ScriptRegistry::Register("Bobber", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<Bobber>()); });
	ScriptRegistry::Register("PlayerController", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<PlayerController>()); });
	ScriptRegistry::Register("FollowCamera", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<FollowCamera>()); });
}
