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
#include <filesystem>

// 클립 이름(부분일치, 소문자)으로 인덱스 검색 — 공격/사망 클립 찾기용
inline int FindClipIdx(ModelAnimator* an, std::initializer_list<const char*> keys)
{
	if (!an) return -1;
	const auto& clips = an->ClipPaths();
	for (const char* k : keys)
		for (int i = 0; i < (int)clips.size(); ++i)
		{
			std::string s = std::filesystem::path(clips[i]).stem().string();
			for (char& c : s) if (c >= 'A' && c <= 'Z') c += 32;
			if (s.find(k) != std::string::npos) return i;
		}
	return -1;
}

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

class EnemyController; // 전방 선언 (PlayerController::DoHit 가 참조)

// ── 3인칭 액션: 플레이어 컨트롤러 (WASD 이동 + Shift 달리기 + 좌클릭/스페이스 공격 + HP) ──
class PlayerController : public MonoBehaviour
{
public:
	float _speed = 3.5f, _runMul = 1.9f, _turn = 12.f;
	float _hp = 100.f, _maxHp = 100.f;
	float _attackDur = 0.6f, _attackRange = 2.4f, _attackDmg = 34.f;
	const char* TypeName() const override { return "PlayerController"; }

	bool IsAttacking() const { return _atkTimer > 0.f; }
	void TakeDamage(float d) { if (_hp <= 0.f) return; _hp -= d; if (_hp < 0.f) _hp = 0.f; }

	void Update() override
	{
		auto t = GetTransform(); if (!t) return;
		auto in = GET_SINGLE(InputManager);
		auto go = GetGameObject();
		auto an = go ? go->GetModelAnimator() : nullptr;

		// ── 공격 ──
		if (_atkCD > 0.f) _atkCD -= DT;
		if (_atkTimer <= 0.f && _atkCD <= 0.f && (in->GetButtonDown(VK_LBUTTON) || in->GetButtonDown(VK_SPACE)))
		{
			if (_slashClip < 0 && an) _slashClip = FindClipIdx(an.get(), { "slash", "attack", "atk", "kick" });
			if (an) { an->SetUseStateMachine(false); if (_slashClip >= 0) an->Play(_slashClip, 0.08f); }
			_atkTimer = _attackDur; _atkCD = _attackDur + 0.15f; _didHit = false;
		}
		if (_atkTimer > 0.f)
		{
			_atkTimer -= DT;
			if (!_didHit && _atkTimer <= _attackDur * 0.55f) { _didHit = true; DoHit(); }
			if (_atkTimer <= 0.f && an) an->SetUseStateMachine(true); // 공격 끝 → 로코모션 복귀
			if (an) an->SetFloat("Speed", 0.f);
			return; // 공격 중 이동 정지
		}

		// ── 이동 ──
		float x = 0, z = 0;
		if (in->GetButton('W')) z += 1.f; if (in->GetButton('S')) z -= 1.f;
		if (in->GetButton('D')) x += 1.f; if (in->GetButton('A')) x -= 1.f;
		float len = sqrtf(x * x + z * z);
		if (len > 0.01f)
		{
			x /= len; z /= len;
			bool run = in->GetButton(VK_SHIFT);
			float spd = _speed * (run ? _runMul : 1.f);
			Vec3 p = t->GetLocalPosition();
			p.x += x * spd * DT; p.z += z * spd * DT;
			t->SetLocalPosition(p);
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

	void DoHit(); // 전방 부채꼴 내 적에게 데미지 (EnemyController 정의 후 구현)

	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Player Controller (Script)");
		ImGui::Text("HP: %.0f / %.0f", _hp, _maxHp);
		ImGui::DragFloat("Move Speed", &_speed, 0.05f, 0.f, 20.f);
		ImGui::DragFloat("Run Mul", &_runMul, 0.02f, 1.f, 4.f);
		ImGui::DragFloat("Attack Range", &_attackRange, 0.05f, 0.5f, 6.f);
		ImGui::DragFloat("Attack Dmg", &_attackDmg, 1.f, 0.f, 200.f);
		ImGui::TextDisabled("Play: WASD 이동 / Shift 달리기 / 좌클릭·Space 공격");
	}
	void Serialize(std::ostream& o) override { o << _speed << ' ' << _runMul << ' ' << _turn << ' ' << _attackDmg; }
	void Deserialize(std::istream& i) override { i >> _speed >> _runMul >> _turn >> _attackDmg; }
private:
	float _atkTimer = 0.f, _atkCD = 0.f; bool _didHit = false; int _slashClip = -1;
};

// ── 적 AI: 플레이어 추적 + 근접 공격 + 피격/사망 ──
class EnemyController : public MonoBehaviour
{
public:
	float _hp = 60.f, _maxHp = 60.f, _speed = 1.9f, _stopDist = 1.7f;
	float _atkCD = 0.f, _atkInterval = 1.3f, _dmg = 10.f;
	bool _dead = false;
	const char* TypeName() const override { return "EnemyController"; }

	void TakeDamage(float d)
	{
		if (_dead) return;
		_hp -= d;
		if (_hp <= 0.f)
		{
			_dead = true; _hp = 0.f;
			if (auto go = GetGameObject()) { if (auto an = go->GetModelAnimator()) an->SetFloat("Speed", 0.f); }
			// 사망 — 쓰러뜨리고 잠시 후 제거 (전용 death 클립 없는 에셋 대응)
			if (auto t = GetTransform()) { Vec3 r = t->GetLocalRotation(); r.x = -1.5708f; t->SetLocalRotation(r); }
			_deadTimer = 2.0f;
		}
	}

	void Update() override
	{
		auto self = GetTransform(); if (!self) return;
		if (_dead) { if ((_deadTimer -= DT) <= 0.f) if (auto go = GetGameObject()) go->SetActive(false); return; }

		auto pl = FindPlayer();
		auto an = GetGameObject() ? GetGameObject()->GetModelAnimator() : nullptr;
		if (!pl) { if (an) an->SetFloat("Speed", 0.f); return; }
		auto pt = pl->GetTransform(); if (!pt) return;
		Vec3 sp = self->GetLocalPosition(), pp = pt->GetLocalPosition();
		Vec3 d{ pp.x - sp.x, 0.f, pp.z - sp.z };
		float dist = sqrtf(d.x * d.x + d.z * d.z);
		if (dist > _stopDist && dist > 1e-3f)
		{
			d.x /= dist; d.z /= dist;
			sp.x += d.x * _speed * DT; sp.z += d.z * _speed * DT;
			self->SetLocalPosition(sp);
			Vec3 r = self->GetLocalRotation(); r.y = atan2f(d.x, d.z); self->SetLocalRotation(r);
			if (an) an->SetFloat("Speed", 1.f);
		}
		else // 근접 → 공격
		{
			if (an) an->SetFloat("Speed", 0.f);
			if (_atkCD > 0.f) _atkCD -= DT;
			else if (auto pc = pl->GetComponent<PlayerController>()) { pc->TakeDamage(_dmg); _atkCD = _atkInterval; }
		}
	}
	void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Enemy Controller (Script)");
		ImGui::Text("HP: %.0f / %.0f%s", _hp, _maxHp, _dead ? "  (DEAD)" : "");
		ImGui::DragFloat("Speed", &_speed, 0.02f, 0.f, 10.f);
		ImGui::DragFloat("Damage", &_dmg, 0.5f, 0.f, 100.f);
	}
	void Serialize(std::ostream& o) override { o << _maxHp << ' ' << _speed << ' ' << _dmg; }
	void Deserialize(std::istream& i) override { i >> _maxHp >> _speed >> _dmg; _hp = _maxHp; }
private:
	float _deadTimer = 0.f;
	shared_ptr<GameObject> FindPlayer()
	{
		auto sc = GET_SINGLE(SceneManager)->GetCurrentScene(); if (!sc) return nullptr;
		for (auto& kv : sc->GetCreatedObjects()) if (kv.second && kv.second->GetObjectName() == L"Player") return kv.second;
		return nullptr;
	}
};

// PlayerController::DoHit — 전방 부채꼴(_attackRange, dot>0.2) 내 살아있는 적에게 데미지
inline void PlayerController::DoHit()
{
	auto self = GetTransform(); auto sc = GET_SINGLE(SceneManager)->GetCurrentScene();
	if (!self || !sc) return;
	Vec3 sp = self->GetLocalPosition(); float yaw = self->GetLocalRotation().y;
	Vec3 fwd{ sinf(yaw), 0.f, cosf(yaw) };
	for (auto& kv : sc->GetCreatedObjects())
	{
		auto& o = kv.second; if (!o || !o->IsActive()) continue;
		auto en = o->GetComponent<EnemyController>(); if (!en || en->_dead) continue;
		auto et = o->GetTransform(); if (!et) continue;
		Vec3 ep = et->GetLocalPosition();
		Vec3 d{ ep.x - sp.x, 0.f, ep.z - sp.z }; float dist = sqrtf(d.x * d.x + d.z * d.z);
		if (dist > _attackRange || dist < 1e-3f) continue;
		if ((d.x * fwd.x + d.z * fwd.z) / dist < 0.2f) continue; // 전방만
		en->TakeDamage(_attackDmg);
	}
}

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
	ScriptRegistry::Register("EnemyController", [] { return std::static_pointer_cast<MonoBehaviour>(std::make_shared<EnemyController>()); });
}
