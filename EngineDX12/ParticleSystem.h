#pragma once
#include "Renderer.h"
#include "Transform.h"
#include "imgui.h"
#include <algorithm>

// DX11 Engine/ParticleSystem 이식 (DX12 — CPU 시뮬레이션, DebugDraw 크로스 렌더).
// per-GameObject 방출기: Sparks/Snow/Fire. 월드 공간 입자.
class ParticleSystem : public Renderer
{
public:
	ParticleSystem() : Renderer(RendererType::Particle) { _renderQueue = RenderQueue::Transparent; }

	struct P { Vec3 pos, vel, col; float life, maxLife; };

	void Update(float dt)
	{
		// 방출기 월드 위치
		Vec3 origin{ 0,0,0 };
		if (auto t = GetTransform()) origin = t->GetLocalPosition();

		// 적분 + 소멸
		for (auto& p : _parts) { p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt; p.pos.z += p.vel.z * dt; p.vel.y += _gravity * dt; p.life -= dt; }
		_parts.erase(std::remove_if(_parts.begin(), _parts.end(), [](const P& p) { return p.life <= 0.f; }), _parts.end());

		if (!_emitting) return;
		_accum += _rate * dt;
		int spawn = (int)_accum; _accum -= spawn;
		for (int i = 0; i < spawn && (int)_parts.size() < _cap; ++i)
		{
			P p; p.maxLife = p.life = _life * (0.6f + 0.8f * rnd());
			float a = rnd() * 6.2831853f, sp = _speed * (0.5f + rnd());
			if (_mode == 0) { p.pos = origin; p.vel = { cosf(a) * sp * 0.4f, sp, sinf(a) * sp * 0.4f }; p.col = _color; }            // Sparks(분수)
			else if (_mode == 1) { p.pos = { origin.x + (rnd() - .5f) * 6.f, origin.y + 4.f, origin.z + (rnd() - .5f) * 6.f }; p.vel = { 0, -sp * 0.3f, 0 }; p.col = { .8f,.85f,1.f }; } // Snow
			else { p.pos = { origin.x + (rnd() - .5f) * .3f, origin.y, origin.z + (rnd() - .5f) * .3f }; p.vel = { (rnd() - .5f) * .5f, sp, (rnd() - .5f) * .5f }; p.col = { 1.f, 0.5f + rnd() * .4f, 0.1f }; } // Fire
			_parts.push_back(p);
		}
	}

	const std::vector<P>& Particles() const { return _parts; }
	float Size() const { return _size; }
	float SizeEnd() const { return _sizeEnd; }
	const Vec3& ColorEnd() const { return _colorEnd; }

	virtual void Draw(const RenderContext&) override {} // 렌더는 D3D12Device::DrawDebugLines 가 처리(크로스)
	virtual void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Particle System");
		const char* modes[] = { "Sparks", "Snow", "Fire" };
		ImGui::Combo("Mode", &_mode, modes, 3);
		ImGui::Checkbox("Emitting", &_emitting);
		ImGui::DragFloat("Rate", &_rate, 1.f, 0.f, 1000.f);
		ImGui::DragFloat("Lifetime", &_life, 0.05f, 0.1f, 20.f);
		ImGui::DragFloat("Speed", &_speed, 0.05f, 0.f, 30.f);
		ImGui::DragFloat("Gravity", &_gravity, 0.05f, -30.f, 30.f);
		ImGui::DragFloat("Size", &_size, 0.005f, 0.005f, 2.f);
		ImGui::DragFloat("Size End", &_sizeEnd, 0.005f, 0.f, 2.f);
		ImGui::ColorEdit3("Color", &_color.x);
		ImGui::ColorEdit3("Color End", &_colorEnd.x);
		ImGui::Text("Alive: %d", (int)_parts.size());
	}

	// 직렬화 파라미터 (월드 입자는 비영속)
	int   _mode = 0;
	bool  _emitting = true;
	float _rate = 60.f, _life = 1.5f, _speed = 3.f, _gravity = -4.f, _size = 0.05f;
	float _sizeEnd = 0.02f;            // 수명 끝 크기(라이프 보간)
	Vec3  _color{ 1.f, 0.7f, 0.2f };
	Vec3  _colorEnd{ 1.f, 0.2f, 0.05f }; // 수명 끝 색(라이프 보간)

private:
	std::vector<P> _parts;
	float _accum = 0.f;
	int   _cap = 600;
	uint32 _seed = 2463534242u;
	float rnd() { _seed = _seed * 1664525u + 1013904223u; return (float)(_seed >> 8) * (1.0f / 16777216.0f); }
};
