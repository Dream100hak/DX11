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
	float MaxDist = 120.f;    // 이 거리에서 완전 소멸
	float FadeRange = 30.f;   // MaxDist 앞 페이드(축소) 구간
	float pad0 = 0.f, pad1 = 0.f, pad2 = 0.f; // 32바이트 정렬
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
	int32 GetVisibleChunks() const { return _visibleChunks; } // 직전 프레임 그린 청크 수
	int32 GetChunkCount() const { return (int32)_chunks.size(); }
	GrassParamsDesc& Params() { return _params; }

private:
	void EnsureResources();

	// 절두체 컬링 단위 — 인스턴스 버퍼의 연속 구간 + 월드 AABB
	struct Chunk
	{
		uint32 start = 0;
		uint32 count = 0;
		BoundingBox box;
	};

	shared_ptr<class VertexBuffer> _quadVB;     // slot 0 — 크로스 쿼드(양면)
	shared_ptr<class IndexBuffer>  _quadIB;
	shared_ptr<class VertexBuffer> _instanceVB; // slot 1 — InstancingData(world+picked), 청크순 정렬
	int32 _count = 0;
	int32 _chunkDim = 16;          // 터레인을 chunkDim×chunkDim 청크로 분할
	int32 _visibleChunks = 0;
	vector<Chunk> _chunks;

	shared_ptr<class HlslShader> _shader;
	shared_ptr<ConstantBuffer<GrassParamsDesc>> _paramsCB;
	GrassParamsDesc _params;
};
