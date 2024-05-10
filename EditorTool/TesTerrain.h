#include "Material.h"

#pragma once
struct VertexTerrain
{
	Vec3 PosL;
	Vec2 Tex;
	Vec2 BoundsY;
};

struct TerrainBuffer
{
	float MinDist;
	float MaxDist;
	float MinTess;
	float MaxTess;
		  
	float TexelCellSpaceU;
	float TexelCellSpaceV;
	float WorldCellSpace;
	float dummy1;

	Vec2 TexScale = Vec2(50.0f , 50.f);
	Vec2 dummy2 = Vec2::Zero;

	Vec4 WorldFrustumPlanes[6];
 
};

class TesTerrain 
{
public:
	struct InitInfo
	{
		std::wstring heightMapFilename;
		std::wstring layerMapFilename0;
		std::wstring layerMapFilename1;
		std::wstring layerMapFilename2;
		std::wstring layerMapFilename3;
		std::wstring layerMapFilename4;
		std::wstring blendMapFilename;
		float heightScale;
		uint32 heightmapWidth;
		uint32 heightmapHeight;
		float cellSpacing;
	};

public:
	TesTerrain();
	~TesTerrain();

	float GetWidth() const
	{
		// Total terrain width.
		return (_info.heightmapWidth - 1) * _info.cellSpacing;
	}

	float GetDepth() const
	{
		// Total terrain depth.
		return (_info.heightmapHeight - 1) * _info.cellSpacing;
	}

	float GetHeight(float x, float z) const
	{
		// Transform from terrain local space to "cell" space.
		float c = (x + 0.5f * GetWidth()) / _info.cellSpacing;
		float d = (z - 0.5f * GetDepth()) / -_info.cellSpacing;

		// Get the row and column we are in.
		int row = (int)floorf(d);
		int col = (int)floorf(c);

		// Grab the heights of the cell we are in.
		// A*--*B
		//  | /|
		//  |/ |
		// C*--*D
		float A = _heightmap[row * _info.heightmapWidth + col];
		float B = _heightmap[row * _info.heightmapWidth + col + 1];
		float C = _heightmap[(row + 1) * _info.heightmapWidth + col];
		float D = _heightmap[(row + 1) * _info.heightmapWidth + col + 1];

		// Where we are relative to the cell.
		float s = c - (float)col;
		float t = d - (float)row;

		// If upper triangle ABC.
		if (s + t <= 1.0f)
		{
			float uy = B - A;
			float vy = C - A;
			return A + s * uy + t * vy;
		}
		else // lower triangle DCB.
		{
			float uy = C - D;
			float vy = B - D;
			return D + (1.0f - s) * uy + (1.0f - t) * vy;
		}
	}

	void SetShader();

	void Init(const InitInfo& initInfo);
	

	void Draw();

	ComPtr<ID3D11ShaderResourceView> GetLayerSRV() { return _layerMapArraySRV; }

private:
	void LoadHeightmap();
	void CreateBuffer();

	void  Smooth();
	bool  InBounds(int32 i, int32 j);
	float Average(int32 i, int32 j);

	void CalcAllPatchBoundsY();
	void CalcPatchBoundsY(uint32 i, uint32 j);
	
	void BuildHeightmapSRV();

private:

	// Divide heightmap into patches such that each patch has CellsPerPatch cells
	// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
	// to 64, we use all the data from the heightmap.  
	static const int CellsPerPatch = 64;

	//Buffer
	shared_ptr<Geometry<VertexTerrain>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	shared_ptr<Texture> _blendMap;

	ComPtr<ID3D11ShaderResourceView> _layerMapArraySRV;
	ComPtr<ID3D11ShaderResourceView> _heightMapSRV;
	
	ComPtr<ID3DX11EffectShaderResourceVariable>  _layerMapArrayEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable>  _blendMapBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable>  _heightMapBuffer;

	InitInfo _info;

	uint32 _numPatchVertices = 0;
	uint32 _numPatchQuadFaces = 0;
	uint32 _numPatchVertRows = 0;
	uint32 _numPatchVertCols = 0;

	std::vector<Vec2> _patchBoundsY;
	std::vector<float> _heightmap;

	TerrainBuffer _terrainDesc;
	shared_ptr<ConstantBuffer<TerrainBuffer>> _terrainBuffer;
	ComPtr<ID3DX11EffectConstantBuffer> _terrainEffectBuffer;

	MaterialDesc _mat; 
};

