#pragma once
#include "ConstantBuffer.h"

// 터레인 식생(잔디) — 인스턴스 쿼드를 GBuffer 로 그린다. Terrain 이 소유하고
// Camera::Render_Deferred Pass 1 에서 터레인 직후 수동 렌더한다.
// 인스턴스 버퍼(수천 개)를 자체 보유 — InstancingManager(캡 500)를 거치지 않음.

struct GrassParamsDesc
{
	float GameTime = 0.f;     // 바람 위상용 누적 시간
	float WindStrength = 0.25f;
	float WindFreq = 1.6f;
	float pad = 0.f;
};

class Terrain;

class Foliage
{
public:
	Foliage();
	~Foliage();

	// 터레인 표면(GetHeight)에 count 개 잔디를 절차적으로 분산 생성
	void Generate(Terrain* terrain, int32 count, float widthScale, float heightScale);
	void Clear();
	void RenderGBuffer(Matrix V, Matrix P, float dt);

	int32 GetCount() const { return _count; }
	GrassParamsDesc& Params() { return _params; }

private:
	void EnsureResources();

	shared_ptr<class VertexBuffer> _quadVB;     // slot 0 — 크로스 쿼드(양면)
	shared_ptr<class IndexBuffer>  _quadIB;
	shared_ptr<class VertexBuffer> _instanceVB; // slot 1 — InstancingData(world+picked)
	int32 _count = 0;

	shared_ptr<class HlslShader> _shader;
	shared_ptr<ConstantBuffer<GrassParamsDesc>> _paramsCB;
	GrassParamsDesc _params;
};
