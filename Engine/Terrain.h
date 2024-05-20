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

struct TerrainBuffer
{
	float FogStart;
	float FogRange;
	Color FogColor;
	Vec2 dummy0;

	float MinDist;
	float MaxDist;
	float MinTess;
	float MaxTess;

	float TexelCellSpaceU;
	float TexelCellSpaceV;
	float WorldCellSpace;
	float dummy1;

	Vec2 TexScale = Vec2(50.0f, 50.f);
	Vec2 dummy2 = Vec2::Zero;

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
	void ChangeShader(shared_ptr<Shader> shader);

	float GetHeight(float x, float z) const;
	shared_ptr<Texture> GetLayerMap() { return _layerMapArray; }

	void TerrainRenderer(shared_ptr<Shader> shader);
	void TerrainRendererNotPS(shared_ptr<Shader> shader);

private:
	
	void CreateInspectorLayerViews();
	void CreateHeightmapSRV();

private:

	shared_ptr<Texture> _blendMap;

	shared_ptr<Texture> _layerMapArray;
	ComPtr<ID3D11ShaderResourceView> _heightMapSRV;

	std::vector<shared_ptr<Texture>> _layerViews; // °ü»ó¿ë

	ComPtr<ID3DX11EffectShaderResourceVariable>  _layerMapArrayEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable>  _blendMapBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable>  _heightMapBuffer;

	TerrainInfo _info;

	shared_ptr<class TerrainMesh> _mesh = nullptr;  

	TerrainBuffer _terrainDesc;
	shared_ptr<ConstantBuffer<TerrainBuffer>> _terrainBuffer;
	ComPtr<ID3DX11EffectConstantBuffer> _terrainEffectBuffer;

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
