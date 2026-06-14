#include "pch.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "TerrainMesh.h"
#include "Terrain.h"
#include "MathUtils.h"


TerrainMesh::TerrainMesh() : Super(ResourceType::Mesh)
{
}
TerrainMesh::~TerrainMesh()
{
}


void TerrainMesh::CreateTerrain(const TerrainInfo& initInfo)
{
	_info = initInfo;

	// Divide heightmap into patches such that each patch has CellsPerPatch.
	_rows = ((_info.heightmapHeight - 1) / CellsPerPatch) + 1;
	_cols = ((_info.heightmapWidth - 1) / CellsPerPatch) + 1;
	_numVertices = _rows * _cols;
	_numIndices = (_cols - 1) * (_cols - 1);

	LoadHeightmap();

	// 편집본(.r32)은 이미 최종 높이 — Smooth 를 돌리면 재로드마다 미세하게 변형되므로 생략.
	{
		std::string ext = std::filesystem::path(_info.heightMapFilename).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (ext != ".r32")
			Smooth();
	}

	CalcAllPatchBoundsY();

	_geometry = make_shared<Geometry<VertexTerrain>>();

	//VERTEX BUFFER
	vector<VertexTerrain> vtx;
	vtx.resize(_rows * _cols);

	float halfWidth = 0.5f * GetWidth();
	float halfDepth = 0.5f * GetDepth();

	float patchWidth = GetWidth() / (_cols - 1);
	float patchDepth = GetDepth() / (_rows - 1);
	float du = 1.0f / (_cols - 1);
	float dv = 1.0f / (_rows - 1);

	for (uint32 i = 0; i < _rows; ++i)
	{
		float z = halfDepth - i * patchDepth;
		for (uint32 j = 0; j < _cols; ++j)
		{
			float x = -halfWidth + j * patchWidth;

			vtx[i * _cols + j].PosL = Vec3(x, 0.0f, z);

			// Stretch texture over grid.
			vtx[i * _cols + j].Tex.x = j * du;
			vtx[i * _cols + j].Tex.y = i * dv;
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (uint32 i = 0; i < _rows - 1; ++i)
	{
		for (uint32 j = 0; j < _cols - 1; ++j)
		{
			uint32 patchID = i * (_cols - 1) + j;
			vtx[i * _cols + j].BoundsY = _patchBoundsY[patchID];
		}
	}

	// Calculate normals
	CalcNormals(vtx);

	_geometry->SetVertices(vtx);

	///////////////////////////////////////////////////

	//INDEX BUFFER 

	std::vector<uint32> indices(_numIndices * 4); // 4 indices per quad face

	// Iterate over each quad and compute indices.
	int32 k = 0;
	for (uint32 i = 0; i < _rows - 1; ++i)
	{
		for (uint32 j = 0; j < _cols - 1; ++j)
		{
			// Top row of 2x2 quad patch
			indices[k] = i * _cols + j;
			indices[k + 1] = i * _cols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[k + 2] = (i + 1) * _cols + j;
			indices[k + 3] = (i + 1) * _cols + j + 1;

			k += 4; // next quad
		}
	}

	_geometry->SetIndices(indices);

	CreateBuffers();
}

void TerrainMesh::RebuildBoundsAndUploadVB()
{
	// 스컬프팅으로 _heightmap 이 바뀐 뒤: 패치별 min/max Y 재계산 → 정점 BoundsY 갱신 → VB 재생성.
	// (정점은 패치 코너만 있어 수십 개 수준 — 매 스트로크 재생성해도 저렴)
	CalcAllPatchBoundsY();

	vector<VertexTerrain> vtx = _geometry->GetVertices(); // const& → 복사 후 수정
	for (uint32 i = 0; i < _rows - 1; ++i)
	{
		for (uint32 j = 0; j < _cols - 1; ++j)
		{
			uint32 patchID = i * (_cols - 1) + j;
			vtx[i * _cols + j].BoundsY = _patchBoundsY[patchID];
		}
	}
	_geometry->SetVertices(vtx);
	_vertexBuffer->Create(_geometry->GetVertices());
}

void TerrainMesh::CreateBuffers()
{
	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());
}

void TerrainMesh::LoadHeightmap()
{
	const size_t count = (size_t)_info.heightmapHeight * _info.heightmapWidth;
	_heightmap.assign(count, 0.f);

	// 확장자 판별 — .r32 = 에디터 저장본(float32 절대 높이), 그 외 = 8-bit RAW(스케일 적용)
	std::string ext = std::filesystem::path(_info.heightMapFilename).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".r32")
	{
		std::ifstream inFile(_info.heightMapFilename.c_str(), std::ios_base::binary);
		if (inFile)
			inFile.read(reinterpret_cast<char*>(_heightmap.data()), (std::streamsize)(count * sizeof(float)));
		return;
	}

	// 기존 8-bit RAW 경로 — 픽셀(0~255)을 heightScale 로 스케일
	std::vector<unsigned char> in(count);
	std::ifstream inFile(_info.heightMapFilename.c_str(), std::ios_base::binary);
	if (inFile)
		inFile.read((char*)&in[0], (std::streamsize)in.size());

	for (size_t i = 0; i < count; ++i)
		_heightmap[i] = (in[i] / 255.0f) * _info.heightScale;
}

void TerrainMesh::Smooth()
{
	std::vector<float> dest(_heightmap.size());

	for (uint32 i = 0; i < _info.heightmapHeight; ++i)
	{
		for (uint32 j = 0; j < _info.heightmapWidth; ++j)
		{
			dest[i * _info.heightmapWidth + j] = Average(i, j);
		}
	}

	// Replace the old heightmap with the filtered one.
	_heightmap = dest;
}

float TerrainMesh::Average(int32 i, int32 j)
{
	// Function computes the average height of the ij element.
// It averages itself with its eight neighbor pixels.  Note
// that if a pixel is missing neighbor, we just don't include it
// in the average--that is, edge pixels don't have a neighbor pixel.
//
// ----------
// | 1| 2| 3|
// ----------
// |4 |ij| 6|
// ----------
// | 7| 8| 9|
// ----------

	float avg = 0.0f;
	float num = 0.0f;

	// Use int to allow negatives.  If we use UINT, @ i=0, m=i-1=UINT_MAX
	// and no iterations of the outer for loop occur.
	for (int32 m = i - 1; m <= i + 1; ++m)
	{
		for (int32 n = j - 1; n <= j + 1; ++n)
		{
			if (InBounds(m, n))
			{
				avg += _heightmap[m * _info.heightmapWidth + n];
				num += 1.0f;
			}
		}
	}

	return avg / num;
}

bool TerrainMesh::InBounds(int32 i, int32 j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int32)_info.heightmapHeight &&
		j >= 0 && j < (int32)_info.heightmapWidth;
}

void TerrainMesh::CalcAllPatchBoundsY()
{
	_patchBoundsY.resize(_numIndices);

	// For each patch
	for (uint32 i = 0; i < _rows - 1; ++i)
	{
		for (uint32 j = 0; j < _cols - 1; ++j)
		{
			CalcPatchBoundsY(i, j);
		}
	}
}

void TerrainMesh::CalcPatchBoundsY(uint32 i, uint32 j)
{
	// Scan the heightmap values this patch covers and compute the min/max height.

	uint32 x0 = j * CellsPerPatch;
	uint32 x1 = (j + 1) * CellsPerPatch;

	uint32 y0 = i * CellsPerPatch;
	uint32 y1 = (i + 1) * CellsPerPatch;

	float minY = +MathUtils::INF;
	float maxY = -MathUtils::INF;

	for (uint32 y = y0; y <= y1; ++y)
	{
		for (uint32 x = x0; x <= x1; ++x)
		{
			uint32 k = y * _info.heightmapWidth + x;
			minY = MathUtils::Min(minY, _heightmap[k]);
			maxY = MathUtils::Max(maxY, _heightmap[k]);
		}
	}

	uint32 patchID = i * (_cols - 1) + j;
	_patchBoundsY[patchID] = Vec2(minY, maxY);
}

void TerrainMesh::CalcNormals(vector<VertexTerrain>& vertices)
{
	for (uint32 i = 0; i < _rows - 1; ++i)
	{
		for (uint32 j = 0; j < _cols - 1; ++j)
		{
			uint32 index0 = i * _cols + j;
			uint32 index1 = i * _cols + j + 1;
			uint32 index2 = (i + 1) * _cols + j;
			uint32 index3 = (i + 1) * _cols + j + 1;

			Vec3 v0 = vertices[index0].PosL;
			Vec3 v1 = vertices[index1].PosL;
			Vec3 v2 = vertices[index2].PosL;
			Vec3 v3 = vertices[index3].PosL;

			Vec3 normal0 = v1 - v0;
			normal0.Cross(v2 - v0);

			Vec3 normal1 = v2 - v1;
			normal1.Cross(v3 - v1);

			vertices[index0].Normal += normal0;
			vertices[index1].Normal += normal0 + normal1;
			vertices[index2].Normal += normal0 + normal1;
			vertices[index3].Normal += normal1;
		}
	}

	// Normalize all normals
	for (auto& vertex : vertices)
	{
		vertex.Normal.Normalize();
	}
}
