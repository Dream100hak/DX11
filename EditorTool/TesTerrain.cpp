#include "pch.h"
#include "TesTerrain.h"
#include <fstream>
#include <sstream>
#include "MathUtils.h"
#include "Camera.h"
#include "Light.h"


TesTerrain::TesTerrain()
{

}
TesTerrain::~TesTerrain()
{

}

void TesTerrain::SetShader()
{
	auto shader = RESOURCES->Get<Shader>(L"Terrain");

	_layerMapArrayEffectBuffer = shader->GetSRV("LayerMapArray");
	_blendMapBuffer = shader->GetSRV("BlendMap");
	_heightMapBuffer = shader->GetSRV("HeightMap");

	if (_terrainEffectBuffer == nullptr)
	{
		_terrainBuffer = make_shared<ConstantBuffer<TerrainBuffer>>();
		_terrainBuffer->Create();
		_terrainEffectBuffer = shader->GetConstantBuffer("TerrainBuffer");
	}

	_mat = {};
	_mat.ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
	_mat.diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	_mat.specular = Color(0.1f, 0.1f, 0.1f, 64.0f);
	_mat.emissive = Color(0.0f, 0.0f, 0.0f, 0.f);

}

void TesTerrain::Init(const InitInfo& initInfo)
{
	_info = initInfo;

	// Divide heightmap into patches such that each patch has CellsPerPatch.
	_numPatchVertRows = ((_info.heightmapHeight - 1) / CellsPerPatch) + 1;
	_numPatchVertCols = ((_info.heightmapWidth - 1) / CellsPerPatch) + 1;

	_numPatchVertices = _numPatchVertRows * _numPatchVertCols;
	_numPatchQuadFaces = (_numPatchVertRows - 1) * (_numPatchVertCols - 1);

	SetShader();
	LoadHeightmap();
	Smooth();
	CalcAllPatchBoundsY();

	CreateBuffer();
	BuildHeightmapSRV();

	std::vector<std::wstring> layerFilenames;
	layerFilenames.push_back(_info.layerMapFilename0);
	layerFilenames.push_back(_info.layerMapFilename1);
	layerFilenames.push_back(_info.layerMapFilename2);
	layerFilenames.push_back(_info.layerMapFilename3);
	layerFilenames.push_back(_info.layerMapFilename4);

	_layerMapArraySRV = Texture::CreateTexture2DArraySRV(layerFilenames);

	_blendMap = RESOURCES->Load<Texture>(_info.blendMapFilename , _info.blendMapFilename);
}

void TesTerrain::Draw()
{
	auto shader = RESOURCES->Get<Shader>(L"Terrain");

	shared_ptr<Scene> scene = CUR_SCENE;
	shared_ptr<Camera> camera = scene->GetMainCamera()->GetCamera();

	Matrix viewProj = camera->GetViewMatrix() * camera->GetProjectionMatrix();

	Vec4 worldPlanes[6];

	MathUtils::ExtractFrustumPlanes(worldPlanes, viewProj);

	//// Set per frame constants.
	Color silver = { 0.75f, 0.75f, 0.75f, 1.0f };

	// GlobalData
	shader->PushMaterialData(_mat);
	shader->PushGlobalData(camera->GetViewMatrix(), camera->GetProjectionMatrix());
	shader->PushLightData(scene->GetLight()->GetLight()->GetLightDesc());

	TerrainBuffer terrainDesc = TerrainBuffer{};
	
	terrainDesc.MinDist = 20.f;
	terrainDesc.MaxDist = 500.f;
	terrainDesc.MinTess = 0.f;
	terrainDesc.MaxTess = 6.f;

	terrainDesc.TexelCellSpaceU = 1.f / _info.heightmapWidth;
	terrainDesc.TexelCellSpaceV = 1.f / _info.heightmapHeight;
	terrainDesc.WorldCellSpace = _info.cellSpacing;

	for (int i = 0; i < 6; ++i)
		terrainDesc.WorldFrustumPlanes[i] = worldPlanes[i];

	_terrainDesc = terrainDesc; 

	_terrainBuffer->CopyData(_terrainDesc);
	_terrainEffectBuffer->SetConstantBuffer(_terrainBuffer->GetComPtr().Get());

	_layerMapArrayEffectBuffer->SetResource(_layerMapArraySRV.Get());
	_blendMapBuffer->SetResource(_blendMap->GetComPtr().Get());
	_heightMapBuffer->SetResource(_heightMapSRV.Get());
	
	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	shader->DrawTerrainIndexed(0, 0, _numPatchQuadFaces * 4, 0, 0);
}

void TesTerrain::CreateBuffer()
{

	//VERTEX BUFFER
	_geometry = make_shared<Geometry<VertexTerrain>>();
	vector<VertexTerrain> vtx;
	vtx.resize(_numPatchVertRows * _numPatchVertCols);

	float halfWidth = 0.5f * GetWidth();
	float halfDepth = 0.5f * GetDepth();

	float patchWidth = GetWidth() / (_numPatchVertCols - 1);
	float patchDepth = GetDepth() / (_numPatchVertRows - 1);
	float du = 1.0f / (_numPatchVertCols - 1);
	float dv = 1.0f / (_numPatchVertRows - 1);

	for (uint32 i = 0; i < _numPatchVertRows; ++i)
	{
		float z = halfDepth - i * patchDepth;
		for (uint32 j = 0; j < _numPatchVertCols; ++j)
		{
			float x = -halfWidth + j * patchWidth;

			vtx[i * _numPatchVertCols + j].PosL = Vec3(x, 0.0f, z);

			// Stretch texture over grid.
			vtx[i * _numPatchVertCols + j].Tex.x = j * du;
			vtx[i * _numPatchVertCols + j].Tex.y = i * dv;
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (uint32 i = 0; i < _numPatchVertRows - 1; ++i)
	{
		for (uint32 j = 0; j < _numPatchVertCols - 1; ++j)
		{
			uint32 patchID = i * (_numPatchVertCols - 1) + j;
			vtx[i * _numPatchVertCols + j].BoundsY = _patchBoundsY[patchID];
		}
	}


	_geometry->SetVertices(vtx);


	///////////////////////////////////////////////////

	//INDEX BUFFER 

	std::vector<uint32> indices(_numPatchQuadFaces * 4); // 4 indices per quad face

	// Iterate over each quad and compute indices.
	int32 k = 0;
	for (uint32 i = 0; i < _numPatchVertRows - 1; ++i)
	{
		for (uint32 j = 0; j < _numPatchVertCols - 1; ++j)
		{
			// Top row of 2x2 quad patch
			indices[k] = i * _numPatchVertCols + j;
			indices[k + 1] = i * _numPatchVertCols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[k + 2] = (i + 1) * _numPatchVertCols + j;
			indices[k + 3] = (i + 1) * _numPatchVertCols + j + 1;

			k += 4; // next quad
		}
	}

	_geometry->SetIndices(indices);

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());

}

void TesTerrain::Smooth()
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

bool TesTerrain::InBounds(int32 i, int32 j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int32)_info.heightmapHeight &&
		j >= 0 && j < (int32)_info.heightmapWidth;
}

float TesTerrain::Average(int32 i, int32 j)
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

void TesTerrain::CalcAllPatchBoundsY()
{
	_patchBoundsY.resize(_numPatchQuadFaces);

	// For each patch
	for (uint32 i = 0; i < _numPatchVertRows - 1; ++i)
	{
		for (uint32 j = 0; j < _numPatchVertCols - 1; ++j)
		{
			CalcPatchBoundsY(i, j);
		}
	}
}

void TesTerrain::CalcPatchBoundsY(uint32 i, uint32 j)
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

	uint32 patchID = i * (_numPatchVertCols - 1) + j;
	_patchBoundsY[patchID] = Vec2(minY, maxY);
}

void TesTerrain::LoadHeightmap()
{
	// A height for each vertex
	std::vector<unsigned char> in(_info.heightmapWidth * _info.heightmapHeight);

	// Open the file.
	std::ifstream inFile;
	inFile.open(_info.heightMapFilename.c_str(), std::ios_base::binary);

	if (inFile)
	{
		// Read the RAW bytes.
		inFile.read((char*)&in[0], (std::streamsize)in.size());

		// Done with file.
		inFile.close();
	}

	// Copy the array data into a float array and scale it.
	_heightmap.resize(_info.heightmapHeight * _info.heightmapWidth, 0);

	for (uint32 i = 0; i < _info.heightmapHeight * _info.heightmapWidth; ++i)
	{
		_heightmap[i] = (in[i] / 255.0f) * _info.heightScale;
	}
}


void TesTerrain::BuildHeightmapSRV()
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = _info.heightmapWidth;
	texDesc.Height = _info.heightmapHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	// HALF is defined in xnamath.h, for storing 16-bit float.
	std::vector<uint16> hmap(_heightmap.size());
	std::transform(_heightmap.begin(), _heightmap.end(), hmap.begin(), MathUtils::ConvertFloatToHalf);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &hmap[0];
	data.SysMemPitch = _info.heightmapWidth * sizeof(uint16);
	data.SysMemSlicePitch = 0;

	HRESULT hr;

	ID3D11Texture2D* hmapTex = 0;
	hr = DEVICE->CreateTexture2D(&texDesc, &data, &hmapTex);
	CHECK(hr);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	
	hr = DEVICE->CreateShaderResourceView(hmapTex, &srvDesc, _heightMapSRV.GetAddressOf());
	CHECK(hr);
}