#pragma once
#include "ResourceBase.h"
#include "Geometry.h"
#include "Terrain.h"

class TerrainMesh : public ResourceBase
{
	using Super = ResourceBase;

public:
	TerrainMesh();
	virtual ~TerrainMesh();

	void CreateTerrain(const TerrainInfo& initInfo);

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

		// 경계 밖(에지 근처 피킹/레이캐스트)에서 OOB 인덱싱 크래시 방지 — 가장자리 셀로 클램프
		if (row < 0) row = 0;
		if (col < 0) col = 0;
		if (row > (int)_info.heightmapHeight - 2) row = (int)_info.heightmapHeight - 2;
		if (col > (int)_info.heightmapWidth  - 2) col = (int)_info.heightmapWidth  - 2;

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


	std::vector<float> GetHeightMap() {return  _heightmap ; }

	// ── 에디터 편집용 ──
	std::vector<float>& HeightMapRef() { return _heightmap; } // 직접 수정용 (스컬프팅)
	uint32 GetHmWidth()  const { return _info.heightmapWidth; }
	uint32 GetHmHeight() const { return _info.heightmapHeight; }
	float  GetCellSpacing() const { return _info.cellSpacing; }
	// 높이맵 수정 후 패치 Y바운드 재계산 + 정점 BoundsY 갱신 + VB 재업로드 (컬링/테셀 정확도 유지)
	void   RebuildBoundsAndUploadVB();

	shared_ptr<VertexBuffer> GetVertexBuffer() { return _vertexBuffer; }
	shared_ptr<IndexBuffer> GetIndexBuffer() { return _indexBuffer; }
	shared_ptr<Geometry<VertexTerrain>> GetGeometry() { return _geometry; }

private:
	void  CreateBuffers();

	void  LoadHeightmap();
	void  Smooth();
	float Average(int32 i, int32 j);
	bool  InBounds(int32 i, int32 j);
	void  CalcAllPatchBoundsY();
	void  CalcPatchBoundsY(uint32 i, uint32 j);
	void  CalcNormals(vector<VertexTerrain>& vertices);

private:
	// Divide heightmap into patches such that each patch has CellsPerPatch cells
// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
// to 64, we use all the data from the heightmap.  
	static const int CellsPerPatch = 64;

	// Mesh
	shared_ptr<Geometry<VertexTerrain>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	std::vector<Vec2>  _patchBoundsY;

	uint32 _rows = 0;
	uint32 _cols = 0;
	uint32 _numVertices = 0;
	uint32 _numIndices = 0;

	std::vector<float> _heightmap;

	TerrainInfo _info;
};

