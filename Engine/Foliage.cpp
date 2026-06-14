#include "pch.h"
#include "Foliage.h"
#include "Terrain.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexData.h"
#include "HlslShader.h"
#include "InstancingBuffer.h" // InstancingData
#include "RenderStateManager.h"

Foliage::Foliage()
{
}

Foliage::~Foliage()
{
}

void Foliage::EnsureResources()
{
	if (_quadVB != nullptr)
		return;

	const float hw = 0.5f, H = 1.0f;

	// 크로스 쿼드 — 수직 두 장(XY/ZY)으로 모든 각도에서 보이게. 노멀은 셰이더가 위쪽으로 덮음.
	vector<VertexTextureNormalData> v(8);
	v[0] = { Vec3(-hw, 0, 0), Vec2(0, 0), Vec3(0, 1, 0) };
	v[1] = { Vec3( hw, 0, 0), Vec2(1, 0), Vec3(0, 1, 0) };
	v[2] = { Vec3( hw, H, 0), Vec2(1, 1), Vec3(0, 1, 0) };
	v[3] = { Vec3(-hw, H, 0), Vec2(0, 1), Vec3(0, 1, 0) };
	v[4] = { Vec3(0, 0, -hw), Vec2(0, 0), Vec3(0, 1, 0) };
	v[5] = { Vec3(0, 0,  hw), Vec2(1, 0), Vec3(0, 1, 0) };
	v[6] = { Vec3(0, H,  hw), Vec2(1, 1), Vec3(0, 1, 0) };
	v[7] = { Vec3(0, H, -hw), Vec2(0, 1), Vec3(0, 1, 0) };

	// 양면(앞/뒤 와인딩) — 백페이스 컬링돼도 두 면 다 보이게
	vector<uint32> idx = {
		0,1,2, 0,2,3, 0,2,1, 0,3,2,
		4,5,6, 4,6,7, 4,6,5, 4,7,6,
	};

	_quadVB = make_shared<VertexBuffer>();
	_quadVB->Create(v, 0, false);
	_quadIB = make_shared<IndexBuffer>();
	_quadIB->Create(idx);

	_shader = RESOURCES->Get<HlslShader>(L"Grass_GBuffer_HLSL");

	_paramsCB = make_shared<ConstantBuffer<GrassParamsDesc>>();
	_paramsCB->Create();
}

void Foliage::Generate(Terrain* terrain, int32 count, float widthScale, float heightScale)
{
	EnsureResources();
	if (terrain == nullptr || count <= 0)
	{
		Clear();
		return;
	}

	const float halfW = 0.5f * terrain->GetWorldWidth();
	const float halfD = 0.5f * terrain->GetWorldDepth();
	const float margin = 1.0f;

	// 결정적 LCG (재현 가능, std::rand 비의존)
	uint32 seed = 1337u;
	auto rnd = [&]() -> float
	{
		seed = seed * 1664525u + 1013904223u;
		return (float)((seed >> 8) & 0xFFFFFF) / 16777216.0f; // 0..1
	};

	vector<InstancingData> inst;
	inst.reserve(count);
	for (int32 i = 0; i < count; ++i)
	{
		float x = -halfW + margin + rnd() * (2.f * halfW - 2.f * margin);
		float z = -halfD + margin + rnd() * (2.f * halfD - 2.f * margin);
		float y = terrain->GetHeight(x, z);
		float yaw = rnd() * 6.2831853f;
		float w = widthScale * (0.7f + 0.6f * rnd());
		float h = heightScale * (0.7f + 0.7f * rnd());

		Matrix world = Matrix::CreateScale(w, h, w)
			* Matrix::CreateRotationY(yaw)
			* Matrix::CreateTranslation(x, y, z);

		InstancingData d;
		d.world = world;
		d.isPicked = 0;
		d.padding[0] = d.padding[1] = d.padding[2] = 0.f;
		inst.push_back(d);
	}

	_count = (int32)inst.size();
	_instanceVB = make_shared<VertexBuffer>();
	_instanceVB->Create(inst, 1, false); // slot 1, immutable
}

void Foliage::Clear()
{
	_count = 0;
	_instanceVB = nullptr;
}

void Foliage::RenderGBuffer(Matrix V, Matrix P, float dt)
{
	if (_count <= 0 || _instanceVB == nullptr || _shader == nullptr)
		return;

	RENDER_STATES->BindAllSamplersVS();
	RENDER_STATES->BindAllSamplersPS();

	_params.GameTime += dt;
	_paramsCB->CopyData(_params);

	_shader->PushGlobalData(V, P);
	_shader->SetVSConstantBuffer(8, _paramsCB->GetComPtr().Get());

	_quadVB->PushData();
	_quadIB->PushData();
	_instanceVB->PushData();

	_shader->DrawIndexedInstanced(_quadIB->GetCount(), _count);
}
