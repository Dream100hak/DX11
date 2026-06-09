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

	void TerrainRenderer(Matrix V, Matrix P);
	void TerrainRendererNotPS(Matrix V, Matrix P);
	void TerrainRendererNormalDepth(Matrix V, Matrix P); // SSAO 입력 (view-space normal+depth)

private:

	void CreateInspectorLayerViews();
	void CreateHeightmapSRV();

private:

	shared_ptr<Texture> _blendMap;

	shared_ptr<Texture> _layerMapArray;
	ComPtr<ID3D11ShaderResourceView> _heightMapSRV;

	std::vector<shared_ptr<Texture>> _layerViews;

	shared_ptr<class HlslShader> _hlslShader;
	shared_ptr<class HlslShader> _hlslShaderShadow;
	shared_ptr<class HlslShader> _hlslShaderNormalDepth;

	TerrainInfo _info;

	shared_ptr<class TerrainMesh> _mesh = nullptr;

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
