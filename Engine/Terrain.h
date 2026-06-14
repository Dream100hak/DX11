#pragma once
#include "Component.h"

struct TerrainInfo
{
	std::wstring heightMapFilename;
	std::wstring blendMapFilename;
	std::vector<wstring> layerMapFilenames;
	float heightScale = 0.f;
	uint32 heightmapWidth = 0;
	uint32 heightmapHeight = 0;
	float cellSpacing = 0.f;
};

// HLSL cbuffer packing (register b8)
// Must match Terrain.hlsl TerrainBuffer layout exactly
struct TerrainBuffer
{
	float FogStart;
	float FogRange;
	Vec2 fogPad;             // padding (HLSL: float4 FogColor crosses 16-byte boundary)

	Color FogColor;

	float MinDist;
	float MaxDist;
	float MinTess;
	float MaxTess;

	float TexelCellSpaceU;
	float TexelCellSpaceV;
	float WorldCellSpace;
	float texPad;

	Vec2 TexScale = Vec2(50.0f, 50.f);
	Vec2 scalePad = Vec2::Zero;

	Vec4 WorldFrustumPlanes[6];
};

class Terrain : public Component
{
	using Super = Component;

public:

	Terrain();
	~Terrain();

	virtual void OnInspectorGUI() override;
	void Update() override;

	void Init(const TerrainInfo& initInfo , shared_ptr<Material> mat);

	float GetHeight(float x, float z) const;
	shared_ptr<Texture> GetLayerMap() { return _layerMapArray; }
	const TerrainInfo& GetInfo() const { return _info; } // 씬 직렬화용

	// ── 에디터 편집 API (TerrainWindow/SceneWindow 브러시) ──
	// 카메라 레이 ↔ 지형(높이필드) 교차 — 브러시 중심 월드 좌표를 찾음. 지형은 원점 정렬 가정.
	bool RaycastTerrain(const Vec3& rayOrigin, const Vec3& rayDir, Vec3& outHit) const;
	// 브러시로 worldHit 주변 높이를 수정 (mode: 0=올림 1=내림 2=스무드 3=평탄화)
	void Sculpt(const Vec3& worldHit, float radius, float strength, int32 mode);
	// 블렌드맵에 레이어 가중치를 칠함 (layer: 0=베이스 ~ 4). 첫 호출 시 기존 블렌드맵을 편집본으로 승격.
	void PaintLayer(const Vec3& worldHit, float radius, float strength, int32 layer);
	// 전체 높이를 height 로 — "평탄 지형" 리셋
	void FlattenAll(float height);
	float GetWorldWidth() const; // (heightmapWidth-1)*cellSpacing
	float GetWorldDepth() const;
	shared_ptr<class TerrainMesh> GetTerrainMesh() { return _mesh; }

	// 편집 결과를 파일로 저장: 높이맵=float32 raw(.r32), 블렌드=.dds.
	// TerrainInfo 파일명을 편집본으로 갱신 → 이후 File>Save Scene 하면 .scene 에 영속되어
	// 다음 로드 때 편집된 지형이 그대로 재생성된다. 성공 시 true.
	bool SaveEditedTerrain();

	// ── 식생(잔디) — Terrain 이 소유, Camera Pass 1 에서 터레인 직후 렌더 ──
	// densityLayer: -1 = 균일, 0~4 = 해당 블렌드 레이어 가중치에 비례해 분산(칠한 곳에만)
	void GenerateFoliage(int32 count, float widthScale, float heightScale, int32 densityLayer = -1);
	void ClearFoliage();
	void RenderFoliageGBuffer(Matrix V, Matrix P, float dt);
	shared_ptr<class Foliage> GetFoliage() { return _foliage; }

	// 월드(x,z)에서 블렌드 레이어 가중치 [0,1] — 식생 밀도용 (layer 0=베이스~4). 블렌드 없으면 1.
	float SampleLayerWeight(float x, float z, int32 layer);

	void TerrainRenderer(Matrix V, Matrix P);
	void TerrainRendererGBuffer(Matrix V, Matrix P);     // ?뷀띁??GBuffer fill (Camera::Render_Deferred Pass 1)
	void TerrainRendererNotPS(Matrix V, Matrix P);
	void TerrainRendererNormalDepth(Matrix V, Matrix P); // SSAO ?낅젰 (view-space normal+depth)

private:

	void CreateInspectorLayerViews();
	void CreateHeightmapSRV();
	void UploadHeightmap(); // _mesh 높이맵 → GPU 텍스처 재업로드 (UpdateSubresource)
	void EnsureEditableBlend(); // 기존 블렌드맵(비압축 BGRA)을 CPU 미러+편집 텍스처로 승격
	void UploadBlend();         // _blendCPU → 편집 블렌드 텍스처 재업로드

private:

	shared_ptr<Texture> _blendMap;

	// 블렌드맵 페인팅 — 기존 텍스처를 CPU 미러 + 편집 텍스처로 승격 (_blendMap SRV 교체)
	std::vector<uint8>      _blendCPU;     // BGRA 픽셀 미러
	uint32                  _blendW = 0;
	uint32                  _blendH = 0;
	ComPtr<ID3D11Texture2D> _blendEditTex; // UpdateSubresource 대상
	bool                    _blendEditable = false;

	shared_ptr<Texture> _layerMapArray;
	ComPtr<ID3D11ShaderResourceView> _heightMapSRV;
	ComPtr<ID3D11Texture2D>          _heightMapTex; // 편집 시 UpdateSubresource 대상 (핸들 보관)

	std::vector<shared_ptr<Texture>> _layerViews;

	shared_ptr<class HlslShader> _hlslShader;
	shared_ptr<class HlslShader> _hlslShaderGBuffer;
	shared_ptr<class HlslShader> _hlslShaderShadow;
	shared_ptr<class HlslShader> _hlslShaderNormalDepth;

	TerrainInfo _info;

	shared_ptr<class TerrainMesh> _mesh = nullptr;
	shared_ptr<class Foliage> _foliage = nullptr; // 식생(잔디)

	TerrainBuffer _terrainDesc;
	shared_ptr<ConstantBuffer<TerrainBuffer>> _terrainBuffer;

	shared_ptr<Material> _mat;

private:

	float _fogStart = 100.f;
	float _fogRange = 300.f;
	Color _fogColor = { 0.69f, 0.77f, 0.87f, 0.0f };


	float _minDist = 20.f;
	float _maxDist = 500.f;
	float _minTess = 0.f;
	float _maxTess = 6.f;
};
