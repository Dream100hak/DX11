#include "pch.h"
#include "Foliage.h"
#include "Terrain.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexData.h"
#include "HlslShader.h"
#include "InstancingBuffer.h" // InstancingData
#include "RenderStateManager.h"
#include "Frustum.h"

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

	if (_kind == FoliageKind::Tree)
		BuildTreeMesh();
	else
		BuildGrassMesh();

	_paramsCB = make_shared<ConstantBuffer<GrassParamsDesc>>();
	_paramsCB->Create();
}

void Foliage::BuildGrassMesh()
{
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
}

void Foliage::BuildTreeMesh()
{
	// 저폴리 나무 — 육각 줄기 기둥(uv.x=0 갈색) + 원뿔 캐노피(uv.x=1 초록).
	// 로컬 단위: 밑동 y=0, 줄기 끝 hT, 캐노피 apex yTop. 인스턴스 스케일로 전체 크기 조절.
	const int32 seg = 6;
	const float rT = 0.10f, hT = 1.1f;   // 줄기 반경/높이
	const float rC = 0.85f, yC0 = 0.8f, yTop = 3.0f; // 캐노피 밑반경/시작높이/끝

	vector<VertexTextureNormalData> v;
	vector<uint32> idx;

	// ── 줄기(육각 기둥) ──
	for (int32 i = 0; i < seg; ++i)
	{
		float a = (float)i / seg * 6.2831853f;
		float nx = cosf(a), nz = sinf(a);
		// 아래/위 링 정점 (uv.x=0)
		v.push_back({ Vec3(nx * rT, 0.f, nz * rT), Vec2(0.f, 0.f), Vec3(nx, 0.f, nz) });
		v.push_back({ Vec3(nx * rT, hT, nz * rT), Vec2(0.f, 1.f), Vec3(nx, 0.f, nz) });
	}
	for (int32 i = 0; i < seg; ++i)
	{
		int32 a0 = i * 2, a1 = i * 2 + 1;
		int32 b0 = ((i + 1) % seg) * 2, b1 = ((i + 1) % seg) * 2 + 1;
		idx.insert(idx.end(), { (uint32)a0,(uint32)b0,(uint32)a1, (uint32)a1,(uint32)b0,(uint32)b1 });
	}

	// ── 캐노피(원뿔) ── apex + 밑링 (uv.x=1)
	uint32 apex = (uint32)v.size();
	v.push_back({ Vec3(0.f, yTop, 0.f), Vec2(1.f, 1.f), Vec3(0.f, 1.f, 0.f) });
	uint32 ringStart = (uint32)v.size();
	for (int32 i = 0; i < seg; ++i)
	{
		float a = (float)i / seg * 6.2831853f;
		float nx = cosf(a), nz = sinf(a);
		Vec3 nrm = Vec3(nx, 0.5f, nz); nrm.Normalize();
		v.push_back({ Vec3(nx * rC, yC0, nz * rC), Vec2(1.f, 0.f), nrm });
	}
	for (int32 i = 0; i < seg; ++i)
	{
		uint32 c0 = ringStart + i;
		uint32 c1 = ringStart + (i + 1) % seg;
		idx.insert(idx.end(), { apex, c0, c1 });
	}

	_quadVB = make_shared<VertexBuffer>();
	_quadVB->Create(v, 0, false);
	_quadIB = make_shared<IndexBuffer>();
	_quadIB->Create(idx);

	_shader = RESOURCES->Get<HlslShader>(L"Tree_GBuffer_HLSL");
}

void Foliage::Generate(Terrain* terrain, int32 count, float widthScale, float heightScale, int32 densityLayer)
{
	EnsureResources();
	Clear();
	if (terrain == nullptr || count <= 0)
		return;

	const float worldW = terrain->GetWorldWidth();
	const float worldD = terrain->GetWorldDepth();
	const float halfW = 0.5f * worldW;
	const float halfD = 0.5f * worldD;
	const float margin = 1.0f;
	const int32 CD = _chunkDim;

	// 결정적 LCG (재현 가능, std::rand 비의존)
	uint32 seed = 1337u;
	auto rnd = [&]() -> float
	{
		seed = seed * 1664525u + 1013904223u;
		return (float)((seed >> 8) & 0xFFFFFF) / 16777216.0f; // 0..1
	};

	// 청크별 버킷 + y 범위 추적 (AABB)
	vector<vector<InstancingData>> buckets(CD * CD);
	vector<float> minY(CD * CD, +1e9f), maxY(CD * CD, -1e9f);

	// densityLayer 지정 시 거부 샘플링(레이어 가중치 비례) — 칠 안 한 곳이 많으면 목표보다 적게 심길 수 있어
	// 시도 횟수에 상한을 둔다.
	int32 placed = 0;
	int32 attempts = 0;
	const int32 maxAttempts = count * 12 + 2000;
	while (placed < count && attempts < maxAttempts)
	{
		++attempts;

		float x = -halfW + margin + rnd() * (2.f * halfW - 2.f * margin);
		float z = -halfD + margin + rnd() * (2.f * halfD - 2.f * margin);

		if (densityLayer >= 0)
		{
			float w = terrain->SampleLayerWeight(x, z, densityLayer);
			w = w * w; // 제곱으로 대비 강화 — 부분적으로 칠한 곳(흙 등)에 잔디가 남는 걸 줄임
			if (rnd() >= w) // 가중치에 비례해 채택
				continue;
		}

		float y = terrain->GetHeight(x, z);
		float yaw = rnd() * 6.2831853f;
		float w = widthScale * (0.7f + 0.6f * rnd());
		float h = heightScale * (0.7f + 0.7f * rnd());

		Matrix world = Matrix::CreateScale(w, h, w)
			* Matrix::CreateRotationY(yaw)
			* Matrix::CreateTranslation(x, y, z);

		int32 cx = (int32)((x + halfW) / worldW * CD); cx = max(0, min(CD - 1, cx));
		int32 cz = (int32)((z + halfD) / worldD * CD); cz = max(0, min(CD - 1, cz));
		int32 ci = cz * CD + cx;

		InstancingData d;
		d.world = world;
		d.isPicked = 0;
		d.padding[0] = d.padding[1] = d.padding[2] = 0.f;
		buckets[ci].push_back(d);

		float top = y + h * 1.5f;
		if (y < minY[ci]) minY[ci] = y;
		if (top > maxY[ci]) maxY[ci] = top;

		++placed;
	}

	// 청크순으로 평탄화 + 구간/AABB 기록 (인스턴스 버퍼는 한 개, 구간만 나눠 그림)
	const float cellW = worldW / CD, cellD = worldD / CD;
	vector<InstancingData> flat;
	flat.reserve(count);
	for (int32 cz = 0; cz < CD; ++cz)
	{
		for (int32 cx = 0; cx < CD; ++cx)
		{
			int32 ci = cz * CD + cx;
			auto& b = buckets[ci];
			if (b.empty())
				continue;

			Chunk c;
			c.start = (uint32)flat.size();
			c.count = (uint32)b.size();

			float x0 = -halfW + cx * cellW, x1 = x0 + cellW;
			float z0 = -halfD + cz * cellD, z1 = z0 + cellD;
			Vec3 mn(x0, minY[ci], z0), mx(x1, maxY[ci], z1);
			BoundingBox::CreateFromPoints(c.box, XMLoadFloat3(&mn), XMLoadFloat3(&mx));

			_chunks.push_back(c);
			flat.insert(flat.end(), b.begin(), b.end());
		}
	}

	_count = (int32)flat.size();
	_instanceVB = make_shared<VertexBuffer>();
	_instanceVB->Create(flat, 1, false); // slot 1, immutable, 청크순 정렬
}

void Foliage::Clear()
{
	_count = 0;
	_visibleChunks = 0;
	_chunks.clear();
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

	// 절두체 + 거리 컬링: 보이는 청크 구간만 인스턴스 드로우
	Frustum frustum;
	frustum.Update(V * P);

	Matrix vinv = V.Invert();
	Vec3 camPos(vinv._41, vinv._42, vinv._43);

	const uint32 idxCount = _quadIB->GetCount();
	int32 visible = 0;
	for (auto& c : _chunks)
	{
		Vec3 center(c.box.Center.x, c.box.Center.y, c.box.Center.z);
		Vec3 ext(c.box.Extents.x, c.box.Extents.y, c.box.Extents.z);
		float radius = ext.Length();

		// 거리 컬링(MaxDist 너머) + 절두체 컬링
		if (Vec3::Distance(camPos, center) - radius > _params.MaxDist)
			continue;
		if (frustum.IsInFrustum(c.box) == false)
			continue;

		_shader->DrawIndexedInstanced(idxCount, c.count, 0, 0, c.start);
		++visible;
	}
	_visibleChunks = visible;
}
