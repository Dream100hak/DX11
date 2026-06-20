#pragma once
#include "Renderer.h"
#include "Transform.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

// DX11 Engine/ParticleSystem 이식 (DX12 — CPU 시뮬레이션, GPU 인스턴스드 빌보드 렌더).
// per-GameObject 방출기: 방출 형태(점/구/콘/박스) + 블렌드(가산/알파) + 버스트 + 수명 곡선.
// 렌더는 D3D12Device::RenderParticles 가 처리(소프트 빌보드 쿼드).
class ParticleSystem : public Renderer
{
public:
	ParticleSystem() : Renderer(RendererType::Particle) { _renderQueue = RenderQueue::Transparent; }

	struct P { Vec3 pos, vel, col; float life, maxLife; };

	enum Shape { ShPoint = 0, ShSphere, ShCone, ShBox };
	enum Blend { BlendAdd = 0, BlendAlpha };

	void Update(float dt)
	{
		Vec3 origin{ 0,0,0 };
		if (auto t = GetTransform()) origin = t->GetLocalPosition();

		// 적분 + 소멸
		for (auto& p : _parts) { p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt; p.pos.z += p.vel.z * dt; p.vel.y += _gravity * dt; p.life -= dt; }
		_parts.erase(std::remove_if(_parts.begin(), _parts.end(), [](const P& p) { return p.life <= 0.f; }), _parts.end());

		// 버스트(즉발) 먼저 소비
		while (_pendingBurst > 0 && (int)_parts.size() < _cap) { Spawn(origin); --_pendingBurst; }

		if (!_emitting) return;
		_accum += _rate * dt;
		int spawn = (int)_accum; _accum -= spawn;
		for (int i = 0; i < spawn && (int)_parts.size() < _cap; ++i) Spawn(origin);
	}

	void Burst() { _pendingBurst += _burst; }

	const std::vector<P>& Particles() const { return _parts; }
	float Size() const { return _size; }
	float SizeEnd() const { return _sizeEnd; }
	const Vec3& ColorEnd() const { return _colorEnd; }
	int   BlendMode() const { return _blend; }
	float Soft() const { return _soft; }
	float FadeIn() const { return _fadeIn; }

	virtual void Draw(const RenderContext&) override {} // 렌더는 D3D12Device::RenderParticles
	virtual void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Particle System");

		// ── 프리셋 (한 번에 그럴듯한 설정) ──
		ImGui::TextUnformatted("Preset:");
		ImGui::SameLine(); if (ImGui::SmallButton("Fire"))      Preset(0);
		ImGui::SameLine(); if (ImGui::SmallButton("Smoke"))     Preset(1);
		ImGui::SameLine(); if (ImGui::SmallButton("Sparks"))    Preset(2);
		ImGui::SameLine(); if (ImGui::SmallButton("Magic"))     Preset(3);
		ImGui::SameLine(); if (ImGui::SmallButton("Explosion")) Preset(4);

		ImGui::Checkbox("Emitting", &_emitting);
		ImGui::SameLine(); ImGui::Text("Alive: %d", (int)_parts.size());

		// 방출
		ImGui::SeparatorText("Emission");
		ImGui::DragFloat("Rate", &_rate, 1.f, 0.f, 2000.f, "%.0f/s");
		const char* shapes[] = { "Point", "Sphere", "Cone", "Box" };
		ImGui::Combo("Shape", &_shape, shapes, 4);
		if (_shape == ShSphere || _shape == ShCone) ImGui::DragFloat("Radius", &_shapeRadius, 0.02f, 0.f, 20.f);
		if (_shape == ShCone) ImGui::DragFloat("Cone Angle", &_coneAngle, 0.5f, 0.f, 90.f, "%.0f deg");
		if (_shape == ShBox)  ImGui::DragFloat3("Box Size", &_boxSize.x, 0.02f, 0.f, 20.f);
		ImGui::DragFloat3("Direction", &_dir.x, 0.02f, -1.f, 1.f);
		ImGui::DragFloat("Spread", &_spread, 0.01f, 0.f, 1.f);
		ImGui::DragFloat("Speed", &_speed, 0.05f, 0.f, 40.f);
		ImGui::DragFloat("Gravity", &_gravity, 0.05f, -30.f, 30.f);
		ImGui::DragFloat("Lifetime", &_life, 0.05f, 0.1f, 30.f, "%.2fs");

		// 버스트
		ImGui::SeparatorText("Burst");
		ImGui::DragInt("Burst Count", &_burst, 1.f, 0, 2000);
		ImGui::SameLine(); if (ImGui::SmallButton("Burst!")) Burst();

		// 외형
		ImGui::SeparatorText("Appearance");
		const char* blends[] = { "Additive (glow)", "Alpha (smoke)" };
		ImGui::Combo("Blend", &_blend, blends, 2);
		ImGui::DragFloat("Softness", &_soft, 0.02f, 0.05f, 6.f, "%.2f"); // <1 넓은글로우, >1 뾰족
		ImGui::DragFloat("Size", &_size, 0.005f, 0.005f, 4.f);
		ImGui::DragFloat("Size End", &_sizeEnd, 0.005f, 0.f, 4.f);
		ImGui::DragFloat("Fade In", &_fadeIn, 0.01f, 0.f, 0.9f); // 수명 초반 알파 페이드인 비율
		ImGui::ColorEdit3("Color", &_color.x);
		ImGui::ColorEdit3("Color End", &_colorEnd.x);
	}

	// ── 직렬화 파라미터 ──
	int   _mode = 0;          // (레거시 pprm 호환 — 미사용)
	bool  _emitting = true;
	float _rate = 60.f, _life = 1.5f, _speed = 3.f, _gravity = -4.f, _size = 0.05f;
	float _sizeEnd = 0.02f;
	Vec3  _color{ 1.f, 0.7f, 0.2f };
	Vec3  _colorEnd{ 1.f, 0.2f, 0.05f };
	// 확장 (pprm2)
	int   _shape = ShCone;
	float _shapeRadius = 0.3f, _coneAngle = 20.f;
	Vec3  _boxSize{ 1,1,1 };
	Vec3  _dir{ 0,1,0 };
	float _spread = 0.3f;
	int   _blend = BlendAdd;
	float _soft = 1.0f;
	float _fadeIn = 0.1f;
	int   _burst = 30;

private:
	std::vector<P> _parts;
	float _accum = 0.f;
	int   _pendingBurst = 0;
	int   _cap = 2000;
	uint32 _seed = 2463534242u;
	float rnd() { _seed = _seed * 1664525u + 1013904223u; return (float)(_seed >> 8) * (1.0f / 16777216.0f); }

	// 균일 단위 구면 벡터
	Vec3 RandUnit()
	{
		float z = rnd() * 2.f - 1.f, a = rnd() * 6.2831853f, r = sqrtf(max(0.f, 1.f - z * z));
		return { r * cosf(a), z, r * sinf(a) };
	}
	// axis 둘레 콘(half-angle deg) 내 균일 방향
	Vec3 ConeDir(Vec3 ax, float angDeg)
	{
		float al = sqrtf(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z) + 1e-6f; ax = { ax.x / al, ax.y / al, ax.z / al };
		float ux = 0, uy = 1, uz = 0; if (fabsf(ax.y) > 0.99f) { ux = 1; uy = 0; uz = 0; }
		float tx = uy * ax.z - uz * ax.y, ty = uz * ax.x - ux * ax.z, tz = ux * ax.y - uy * ax.x;
		float tl = sqrtf(tx * tx + ty * ty + tz * tz) + 1e-6f; tx /= tl; ty /= tl; tz /= tl;
		float bx = ax.y * tz - ax.z * ty, by = ax.z * tx - ax.x * tz, bz = ax.x * ty - ax.y * tx;
		float cz = 1.f - rnd() * (1.f - cosf(angDeg * 3.14159265f / 180.f));
		float sr = sqrtf(max(0.f, 1.f - cz * cz)), phi = rnd() * 6.2831853f, cp = cosf(phi), sp = sinf(phi);
		return { ax.x * cz + (tx * cp + bx * sp) * sr, ax.y * cz + (ty * cp + by * sp) * sr, ax.z * cz + (tz * cp + bz * sp) * sr };
	}

	void Spawn(const Vec3& origin)
	{
		P p; p.maxLife = p.life = _life * (0.7f + 0.6f * rnd());
		Vec3 d = _dir; float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z); if (dl > 1e-4f) { d.x /= dl; d.y /= dl; d.z /= dl; } else d = { 0,1,0 };
		float sp = _speed * (0.6f + 0.8f * rnd());
		Vec3 pos = origin, vel{ 0,0,0 };
		switch (_shape)
		{
		case ShSphere: { Vec3 u = RandUnit(); pos = { origin.x + u.x * _shapeRadius, origin.y + u.y * _shapeRadius, origin.z + u.z * _shapeRadius }; vel = { u.x * sp, u.y * sp, u.z * sp }; break; }
		case ShCone:   { Vec3 c = ConeDir(d, _coneAngle); vel = { c.x * sp, c.y * sp, c.z * sp }; break; }
		case ShBox:    { pos = { origin.x + (rnd() - .5f) * _boxSize.x, origin.y + (rnd() - .5f) * _boxSize.y, origin.z + (rnd() - .5f) * _boxSize.z }; vel = { d.x * sp, d.y * sp, d.z * sp }; break; }
		default:       { vel = { d.x * sp, d.y * sp, d.z * sp }; break; } // Point
		}
		// 스프레드(방향 랜덤화)
		if (_spread > 0.f) { Vec3 u = RandUnit(); vel.x += u.x * _spread * sp; vel.y += u.y * _spread * sp; vel.z += u.z * _spread * sp; }
		p.pos = pos; p.vel = vel; p.col = _color;
		_parts.push_back(p);
	}

	void Preset(int k)
	{
		switch (k)
		{
		case 0: // Fire
			_shape = ShCone; _coneAngle = 18.f; _dir = { 0,1,0 }; _spread = 0.15f; _rate = 120.f; _life = 1.1f;
			_speed = 1.6f; _gravity = 1.2f; _size = 0.18f; _sizeEnd = 0.02f; _soft = 0.8f; _blend = BlendAdd; _fadeIn = 0.1f;
			_color = { 1.f, 0.7f, 0.2f }; _colorEnd = { 1.f, 0.15f, 0.03f }; break;
		case 1: // Smoke
			_shape = ShCone; _coneAngle = 22.f; _dir = { 0,1,0 }; _spread = 0.25f; _rate = 30.f; _life = 3.5f;
			_speed = 0.8f; _gravity = 0.3f; _size = 0.25f; _sizeEnd = 1.1f; _soft = 0.5f; _blend = BlendAlpha; _fadeIn = 0.25f;
			_color = { 0.30f,0.30f,0.32f }; _colorEnd = { 0.08f,0.08f,0.09f }; break;
		case 2: // Sparks
			_shape = ShCone; _coneAngle = 35.f; _dir = { 0,1,0 }; _spread = 0.2f; _rate = 80.f; _life = 0.9f;
			_speed = 6.f; _gravity = -9.f; _size = 0.05f; _sizeEnd = 0.01f; _soft = 3.0f; _blend = BlendAdd; _fadeIn = 0.02f;
			_color = { 1.f,0.9f,0.5f }; _colorEnd = { 1.f,0.4f,0.05f }; break;
		case 3: // Magic
			_shape = ShSphere; _shapeRadius = 0.4f; _spread = 0.1f; _rate = 60.f; _life = 1.8f;
			_speed = 0.6f; _gravity = 0.2f; _size = 0.12f; _sizeEnd = 0.0f; _soft = 1.5f; _blend = BlendAdd; _fadeIn = 0.15f;
			_color = { 0.4f,0.7f,1.f }; _colorEnd = { 0.7f,0.3f,1.f }; break;
		case 4: // Explosion (버스트 위주)
			_shape = ShSphere; _shapeRadius = 0.1f; _spread = 0.4f; _rate = 0.f; _life = 1.2f; _burst = 200;
			_speed = 8.f; _gravity = -3.f; _size = 0.22f; _sizeEnd = 0.03f; _soft = 1.2f; _blend = BlendAdd; _fadeIn = 0.02f;
			_color = { 1.f,0.8f,0.4f }; _colorEnd = { 1.f,0.2f,0.05f }; Burst(); break;
		}
	}
};
