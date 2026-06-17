#pragma once
#include "Renderer.h"

class D3D12Device;
class Terrain;

// 터레인 식생 — 잔디 블레이드 + 저폴리 나무를 월드공간 정점으로 베이크해 단일 VB/IB 로 렌더.
//   자체 RendererType(Foliage) → GetMeshRenderer 회피 → RT TLAS 자동 제외(대용량 OOB 크래시 회피).
//   메인 디퍼드 PSO + 정점색(mode 2). 생성 파라미터(시드/개수)만 .scene 저장 → 로드 시 결정적 재생성.
class Foliage : public Renderer
{
public:
	Foliage() : Renderer(RendererType::Foliage) {}
	void Bind(D3D12Device* dev) { _dev = dev; }

	// 터레인 표면에 산포(GetHeight 샘플). seed 동일 시 결과 동일(결정적).
	void Generate(Terrain* terrain, int grassCount, int treeCount, float grassSize, uint32 seed);
	void Regenerate(Terrain* terrain) { Generate(terrain, _grassCount, _treeCount, _grassSize, _seed); }

	virtual void Draw(const RenderContext& ctx) override;
	virtual void TransformBoundingBox() override; // 베이크된 정점 AABB

	int   GrassCount() const { return _grassCount; }
	int   TreeCount() const { return _treeCount; }
	float GrassSize() const { return _grassSize; }
	uint32 Seed() const { return _seed; }

private:
	void Upload();

	D3D12Device*           _dev = nullptr;
	int                    _grassCount = 4000;
	int                    _treeCount = 60;
	float                  _grassSize = 0.4f;
	uint32                 _seed = 1337;

	vector<Vtx>            _verts;
	vector<uint32>         _indices;
	ComPtr<ID3D12Resource> _vb, _ib;
	void*                  _vbMapped = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};
	DirectX::XMFLOAT3      _aabbMin{ 0,0,0 }, _aabbMax{ 0,0,0 };
};
