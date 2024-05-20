#include "pch.h"
#include <filesystem>
#include "Terrain.h"
#include "MathUtils.h"
#include "Camera.h"
#include "Light.h"
#include "TerrainMesh.h"
#include "Material.h"


Terrain::Terrain() : Super(ComponentType::Terrain)
{
}

Terrain::~Terrain()
{
}

void Terrain::OnInspectorGUI()
{
	Super::OnInspectorGUI();

	ImGui::DragFloat("Fog Start", (float*)&_fogStart, 0.1f);
	ImGui::DragFloat("Fog Range", (float*)&_fogRange, 0.1f);
	ImGui::ColorEdit4("Fog Color",(float*)&_fogColor);

	if (_mat != nullptr)
	{
		MaterialDesc& desc = _mat->GetMaterialDesc();

		if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) {}
		if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) {}
		if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) {}
		if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) {}
	}

	ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

	ImGui::DragFloat("Min Dist", (float*)&_minDist, 0.1f);
	ImGui::DragFloat("Max Dist", (float*)&_maxDist, 0.1f);
	ImGui::DragFloat("Min Tess", (float*)&_minTess, 0.1f);
	ImGui::DragFloat("Max Tess", (float*)&_maxTess, 0.1f);
	ImGui::DragFloat("Cell Spacing", (float*)&_info.cellSpacing, 0.01f);

	ImGui::Separator();

	ImGui::Text("Layer Textures"); 

	for (int32 i = 0; i < _layerViews.size(); ++i)
	{
		ImGui::Image(_layerViews[i]->GetComPtr().Get(), ImVec2(55, 55));
		if(i < _layerViews.size() - 1)
			ImGui::SameLine();
	}

	ImGui::Separator();

	ImGui::BeginGroup();
	ImGui::Image(_heightMapSRV.Get(), ImVec2(75, 75));
	ImGui::TextColored(color, "Height Map");
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginGroup();
	ImGui::Image(_blendMap->GetComPtr().Get(), ImVec2(75, 75));
	ImGui::TextColored(color, "Blend Map");
	ImGui::EndGroup();
}

void Terrain::ChangeShader(shared_ptr<Shader> shader)
{
	_layerMapArrayEffectBuffer = shader->GetSRV("LayerMapArray");
	_blendMapBuffer = shader->GetSRV("BlendMap");
	_heightMapBuffer = shader->GetSRV("HeightMap");
	_terrainEffectBuffer = shader->GetConstantBuffer("TerrainBuffer");
}

float Terrain::GetHeight(float x, float z) const
{
	 return _mesh->GetHeight(x, z);
}

void Terrain::Init(const TerrainInfo& initInfo , shared_ptr<Material> mat)
{
	_info = initInfo;
	_mat = mat;

	_mesh = make_shared<class TerrainMesh>();
	_mesh->CreateTerrain(initInfo);

	_terrainBuffer = make_shared<ConstantBuffer<TerrainBuffer>>();
	_terrainBuffer->Create();

	CreateHeightmapSRV();
	CreateInspectorLayerViews();

	_layerMapArray = make_shared<Texture>();
	_layerMapArray->CreateTexture2DArraySRV(_info.layerMapFilenames);

	_blendMap = RESOURCES->Load<Texture>(_info.blendMapFilename, _info.blendMapFilename);
}

void Terrain::Update()
{
	if (INPUT->GetButton(KEY_TYPE::KEY_1))
		DCT->RSSetState(GRAPHICS->GetWireframeRS().Get());

	auto shader = RESOURCES->Get<Shader>(L"Terrain");
	TerrainRenderer(shader);

	DCT->RSSetState(0);
}

void Terrain::TerrainRenderer(shared_ptr<Shader> shader)
{

	ChangeShader(shader);
	shared_ptr<Scene> scene = CUR_SCENE;
	shared_ptr<Camera> camera = scene->GetMainCamera()->GetCamera();

	Matrix viewProj = camera->GetViewMatrix() * camera->GetProjectionMatrix();

	Vec4 worldPlanes[6];

	MathUtils::ExtractFrustumPlanes(worldPlanes, viewProj);

	// GlobalData
	_mat->SetShader(shader);
	_mat->Update();

	shader->PushGlobalData(camera->GetViewMatrix(), camera->GetProjectionMatrix());
	shader->PushLightData(scene->GetLight()->GetLight()->GetLightDesc());

	TerrainBuffer terrainDesc = TerrainBuffer{};

	terrainDesc.FogStart = _fogStart;
	terrainDesc.FogRange = _fogRange;
	terrainDesc.FogColor = _fogColor;

	terrainDesc.MinDist = _minDist;
	terrainDesc.MaxDist = _maxDist;
	terrainDesc.MinTess = _minTess;
	terrainDesc.MaxTess = _maxTess;

	terrainDesc.TexelCellSpaceU = 1.f / _info.heightmapWidth;
	terrainDesc.TexelCellSpaceV = 1.f / _info.heightmapHeight;
	terrainDesc.WorldCellSpace = _info.cellSpacing;

	for (int i = 0; i < 6; ++i)
		terrainDesc.WorldFrustumPlanes[i] = worldPlanes[i];

	_terrainDesc = terrainDesc;

	_terrainBuffer->CopyData(_terrainDesc);
	_terrainEffectBuffer->SetConstantBuffer(_terrainBuffer->GetComPtr().Get());

	_layerMapArrayEffectBuffer->SetResource(_layerMapArray->GetComPtr().Get());
	_blendMapBuffer->SetResource(_blendMap->GetComPtr().Get());
	_heightMapBuffer->SetResource(_heightMapSRV.Get());

	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	shader->DrawTerrainIndexed(0, 0, _mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);
}

void Terrain::TerrainRendererNotPS(shared_ptr<Shader> shader)
{
	ChangeShader(shader);

	shared_ptr<Scene> scene = CUR_SCENE;
	shared_ptr<Light> light = scene->GetLight()->GetLight();
	shared_ptr<Camera> camera = scene->GetMainCamera()->GetCamera();

	Matrix V = light->S_MatView;
	Matrix P = light->S_MatProjection;

	// GlobalData
	shader->PushGlobalData(V, P);
	TerrainBuffer terrainDesc = TerrainBuffer{};

	terrainDesc.MinDist = _minDist;
	terrainDesc.MaxDist = _maxDist;
	terrainDesc.MinTess = _minTess;
	terrainDesc.MaxTess = _maxTess;

	_terrainDesc = terrainDesc;

	_terrainBuffer->CopyData(_terrainDesc);
	_terrainEffectBuffer->SetConstantBuffer(_terrainBuffer->GetComPtr().Get());

	_heightMapBuffer->SetResource(_heightMapSRV.Get());

	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	shader->DrawTerrainIndexed(1, 0, _mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);
}

void Terrain::CreateInspectorLayerViews()
{
	for (int32 i = 0; i < _info.layerMapFilenames.size(); i++) 
	{	
		shared_ptr<Texture> tex = make_shared<Texture>();
		tex->Load(_info.layerMapFilenames[i]);
		_layerViews.push_back(tex);
	}
}

void Terrain::CreateHeightmapSRV()
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

	std::vector<float> heightmap = _mesh->GetHeightMap();

	// HALF is defined in xnamath.h, for storing 16-bit float.
	std::vector<uint16> hmap(heightmap.size());
	std::transform(heightmap.begin(), heightmap.end(), hmap.begin(), MathUtils::ConvertFloatToHalf);

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
