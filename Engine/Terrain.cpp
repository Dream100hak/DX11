#include "pch.h"
#include <filesystem>
#include "Terrain.h"
#include "MathUtils.h"
#include "Camera.h"
#include "Light.h"
#include "TerrainMesh.h"
#include "Material.h"
#include "HlslShader.h"
#include "RenderStateManager.h"


Terrain::Terrain() : Super(ComponentType::Terrain)
{
}

Terrain::~Terrain()
{
}

void Terrain::OnInspectorGUI()
{
	Super::OnInspectorGUI();

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

	_hlslShader = RESOURCES->Get<HlslShader>(L"Terrain_HLSL");
	_hlslShaderGBuffer = RESOURCES->Get<HlslShader>(L"Terrain_GBuffer_HLSL");
	_hlslShaderShadow = RESOURCES->Get<HlslShader>(L"Terrain_Shadow_HLSL");
	_hlslShaderNormalDepth = RESOURCES->Get<HlslShader>(L"Terrain_NormalDepth_HLSL");
}

void Terrain::Update()
{
	// ?쒕줈?곕뒗 Camera::Render_Deferred Pass 1 ?먯꽌 TerrainRendererGBuffer 濡??섑뻾
	// (?덉쟾???ш린???ъ썙?쒕줈 諛깅쾭?쇱뿉 吏곸젒 洹몃졇?????뷀띁???쇱씠??PBR/SSAO ?쇨큵 ?곸슜 ?꾪빐 GBuffer 濡??몄엯)
}

void Terrain::TerrainRenderer(Matrix V, Matrix P)
{
	auto shader = _hlslShader;
	if (!shader) return;

	Vec4 worldPlanes[6];
	MathUtils::ExtractFrustumPlanes(worldPlanes, V * P);

	// Bind samplers to all stages that sample textures
	RENDER_STATES->BindAllSamplersVS();
	RENDER_STATES->BindAllSamplersPS();
	RENDER_STATES->BindAllSamplersDS();

	// GlobalData (b0) -> VS, HS, DS, PS
	shader->PushGlobalData(V, P);

	// LightData (b2) -> PS — 라이트 없는 씬은 기본값 (null 역참조 크래시 방지)
	{
		auto lightObj = CUR_SCENE->GetLight();
		LightDesc lightDesc;
		if (lightObj != nullptr && lightObj->GetLight() != nullptr)
			lightDesc = lightObj->GetLight()->GetLightDesc();
		shader->PushLightData(lightDesc);
	}

	// MaterialData (b3) -> PS
	if (_mat)
		shader->PushMaterialData(_mat->GetMaterialDesc());

	// TerrainBuffer (b8) -> HS, DS, PS
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
	auto terrainBuf = _terrainBuffer->GetComPtr().Get();
	shader->SetHSConstantBuffer(8, terrainBuf);
	shader->SetDSConstantBuffer(8, terrainBuf);
	shader->SetPSConstantBuffer(8, terrainBuf);

	// SRV bindings
	// t0: LayerMapArray (PS)
	shader->SetPSSRV(0, _layerMapArray->GetComPtr().Get());
	// t1: BlendMap (PS)
	shader->SetPSSRV(1, _blendMap->GetComPtr().Get());
	// t2: HeightMap (VS + DS + PS)
	shader->SetVSSRV(2, _heightMapSRV.Get());
	shader->SetDSSRV(2, _heightMapSRV.Get());
	shader->SetPSSRV(2, _heightMapSRV.Get());
	// t3: ShadowMap (PS)
	if (_mat && _mat->GetShadowMap())
		shader->SetPSSRV(3, _mat->GetShadowMap()->GetComPtr().Get());

	// Vertex/Index buffer
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	// Draw with 4-control-point patch list topology
	shader->DrawTerrainIndexed(_mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);

	// Cleanup HS/DS to avoid affecting subsequent draws
	shader->Unbind();
}

// ?뷀띁??GBuffer fill ??Camera::Render_Deferred Pass 1 (GBuffer MRT 諛붿씤???곹깭) ?먯꽌 ?몄텧
// ?쇱씠??洹몃┝??SSAO ???뷀띁???쇱씠???⑥뒪媛 ?쇨큵 泥섎━?섎?濡?albedo/normal/position 留?湲곕줉
void Terrain::TerrainRendererGBuffer(Matrix V, Matrix P)
{
	auto shader = _hlslShaderGBuffer;
	if (!shader) return;

	Vec4 worldPlanes[6];
	MathUtils::ExtractFrustumPlanes(worldPlanes, V * P);

	// KEY_1: ??댁뼱?꾨젅???붾쾭洹?(援?Terrain::Update ?숈옉 ?좎?)
	if (INPUT->GetButton(KEY_TYPE::KEY_1))
		DCT->RSSetState(GRAPHICS->GetWireframeRS().Get());

	RENDER_STATES->BindAllSamplersVS();
	RENDER_STATES->BindAllSamplersPS();
	RENDER_STATES->BindAllSamplersDS();

	// GlobalData (b0) -> VS, HS, DS, PS
	shader->PushGlobalData(V, P);

	// MaterialData (b3) -> PS (Roughness/Metallic)
	if (_mat)
		shader->PushMaterialData(_mat->GetMaterialDesc());

	// TerrainBuffer (b8) -> HS, DS, PS
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
	auto terrainBuf = _terrainBuffer->GetComPtr().Get();
	shader->SetHSConstantBuffer(8, terrainBuf);
	shader->SetDSConstantBuffer(8, terrainBuf);
	shader->SetPSConstantBuffer(8, terrainBuf);

	// SRV bindings
	shader->SetPSSRV(0, _layerMapArray->GetComPtr().Get());
	shader->SetPSSRV(1, _blendMap->GetComPtr().Get());
	shader->SetVSSRV(2, _heightMapSRV.Get());
	shader->SetDSSRV(2, _heightMapSRV.Get());
	shader->SetPSSRV(2, _heightMapSRV.Get());

	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	shader->DrawTerrainIndexed(_mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);

	shader->Unbind();

	if (INPUT->GetButton(KEY_TYPE::KEY_1))
		DCT->RSSetState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullBack).Get());
}

void Terrain::TerrainRendererNotPS(Matrix V, Matrix P)
{
	auto shader = _hlslShaderShadow;
	if (!shader) return;

	// Bind samplers for VS/DS HeightMap sampling
	RENDER_STATES->BindAllSamplersVS();
	RENDER_STATES->BindAllSamplersDS();

	// GlobalData (b0) -> VS, HS, DS
	shader->PushGlobalData(V, P);

	// TerrainBuffer (b8) -> HS, DS (tess factors only, no frustum cull for shadow)
	TerrainBuffer terrainDesc = TerrainBuffer{};

	terrainDesc.MinDist = _minDist;
	terrainDesc.MaxDist = _maxDist;
	terrainDesc.MinTess = _minTess;
	terrainDesc.MaxTess = _maxTess;

	_terrainBuffer->CopyData(terrainDesc);
	auto terrainBuf = _terrainBuffer->GetComPtr().Get();
	shader->SetHSConstantBuffer(8, terrainBuf);
	shader->SetDSConstantBuffer(8, terrainBuf);

	// t2: HeightMap (VS + DS)
	shader->SetVSSRV(2, _heightMapSRV.Get());
	shader->SetDSSRV(2, _heightMapSRV.Get());

	// Vertex/Index buffer
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	shader->DrawTerrainIndexed(_mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);

	shader->Unbind();
}

// SSAO normal-depth ?⑥뒪: view-space normal + depth 湲곕줉 (PS_NormalDepth)
void Terrain::TerrainRendererNormalDepth(Matrix V, Matrix P)
{
	auto shader = _hlslShaderNormalDepth;
	if (!shader) return;

	// VS/DS HeightMap ?섑뵆留?+ PS ?몃? 怨꾩궛???섑뵆??
	RENDER_STATES->BindAllSamplersVS();
	RENDER_STATES->BindAllSamplersDS();
	RENDER_STATES->BindAllSamplersPS();

	// GlobalData (b0) -> VS, HS, DS, PS (PS ??V 濡?view-space 蹂??
	shader->PushGlobalData(V, P);

	// TerrainBuffer (b8) -> HS, DS (?뚯??덉씠?? + PS (TexelCellSpace/WorldCellSpace ?몃? 怨꾩궛)
	TerrainBuffer terrainDesc = TerrainBuffer{};

	terrainDesc.MinDist = _minDist;
	terrainDesc.MaxDist = _maxDist;
	terrainDesc.MinTess = _minTess;
	terrainDesc.MaxTess = _maxTess;

	terrainDesc.TexelCellSpaceU = 1.f / _info.heightmapWidth;
	terrainDesc.TexelCellSpaceV = 1.f / _info.heightmapHeight;
	terrainDesc.WorldCellSpace = _info.cellSpacing;

	_terrainBuffer->CopyData(terrainDesc);
	auto terrainBuf = _terrainBuffer->GetComPtr().Get();
	shader->SetHSConstantBuffer(8, terrainBuf);
	shader->SetDSConstantBuffer(8, terrainBuf);
	shader->SetPSConstantBuffer(8, terrainBuf);

	// t2: HeightMap (VS + DS + PS)
	shader->SetVSSRV(2, _heightMapSRV.Get());
	shader->SetDSSRV(2, _heightMapSRV.Get());
	shader->SetPSSRV(2, _heightMapSRV.Get());

	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	shader->DrawTerrainIndexed(_mesh->GetIndexBuffer()->GetCount() * 4, 0, 0);

	shader->Unbind();
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

	std::vector<uint16> hmap(heightmap.size());
	std::transform(heightmap.begin(), heightmap.end(), hmap.begin(), MathUtils::ConvertFloatToHalf);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &hmap[0];
	data.SysMemPitch = _info.heightmapWidth * sizeof(uint16);
	data.SysMemSlicePitch = 0;

	HRESULT hr;

	_heightMapTex.Reset();
	hr = DEVICE->CreateTexture2D(&texDesc, &data, _heightMapTex.GetAddressOf()); // 핸들 보관 (편집 시 UpdateSubresource)
	CHECK(hr);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	hr = DEVICE->CreateShaderResourceView(_heightMapTex.Get(), &srvDesc, _heightMapSRV.GetAddressOf());
	CHECK(hr);
}

void Terrain::UploadHeightmap()
{
	if (_heightMapTex == nullptr || _mesh == nullptr)
		return;

	auto& hm = _mesh->HeightMapRef();
	std::vector<uint16> hmap(hm.size());
	std::transform(hm.begin(), hm.end(), hmap.begin(), MathUtils::ConvertFloatToHalf);

	DCT->UpdateSubresource(_heightMapTex.Get(), 0, nullptr,
		hmap.data(), _info.heightmapWidth * sizeof(uint16), 0);
}

float Terrain::GetWorldWidth() const { return _mesh ? (_info.heightmapWidth  - 1) * _info.cellSpacing : 0.f; }
float Terrain::GetWorldDepth() const { return _mesh ? (_info.heightmapHeight - 1) * _info.cellSpacing : 0.f; }

bool Terrain::RaycastTerrain(const Vec3& o, const Vec3& d, Vec3& outHit) const
{
	if (_mesh == nullptr)
		return false;

	const float halfW = 0.5f * GetWorldWidth();
	const float halfD = 0.5f * GetWorldDepth();
	const float step = max(_info.cellSpacing * 0.5f, 0.05f);
	const float maxDist = 8000.f;

	auto inside = [&](const Vec3& p) {
		return p.x >= -halfW && p.x <= halfW && p.z >= -halfD && p.z <= halfD;
	};

	float prevT = 0.f;
	bool  havePrev = false;
	for (float t = 0.f; t < maxDist; t += step)
	{
		Vec3 p = o + d * t;
		if (inside(p) == false)
		{
			havePrev = false; // 지형 밖 — 표면 비교 생략 (에지 클램프 오판 방지)
			continue;
		}

		float h = _mesh->GetHeight(p.x, p.z);
		float diff = p.y - h; // >0 위, <=0 표면 아래(교차)
		if (diff <= 0.f)
		{
			float a = havePrev ? prevT : max(0.f, t - step);
			float b = t;
			for (int it = 0; it < 20; ++it) // 이분 정밀화
			{
				float m = (a + b) * 0.5f;
				Vec3 pm = o + d * m;
				float hm = _mesh->GetHeight(pm.x, pm.z);
				if (pm.y - hm <= 0.f) b = m; else a = m;
			}
			outHit = o + d * b;
			return true;
		}
		prevT = t;
		havePrev = true;
	}
	return false;
}

void Terrain::Sculpt(const Vec3& worldHit, float radius, float strength, int32 mode)
{
	if (_mesh == nullptr || radius <= 0.f)
		return;

	auto& hm = _mesh->HeightMapRef();
	const int32 W = (int32)_info.heightmapWidth;
	const int32 H = (int32)_info.heightmapHeight;
	const float cs = _info.cellSpacing;
	const float halfW = 0.5f * GetWorldWidth();
	const float halfD = 0.5f * GetWorldDepth();

	// 월드 → 셀 인덱스 (GetHeight 의 역변환과 동일: col=(x+halfW)/cs, row=(halfD-z)/cs)
	const float cf = (worldHit.x + halfW) / cs;
	const float rf = (halfD - worldHit.z) / cs;
	const int32 cc = (int32)lroundf(cf);
	const int32 cr = (int32)lroundf(rf);
	const int32 rad = (int32)ceilf(radius / cs) + 1;

	const float target = _mesh->GetHeight(worldHit.x, worldHit.z); // 평탄화 기준 높이

	for (int32 i = cr - rad; i <= cr + rad; ++i)
	{
		if (i < 0 || i >= H) continue;
		for (int32 j = cc - rad; j <= cc + rad; ++j)
		{
			if (j < 0 || j >= W) continue;

			float wx = -halfW + j * cs;
			float wz = halfD - i * cs;
			float dx = wx - worldHit.x, dz = wz - worldHit.z;
			float dist = sqrtf(dx * dx + dz * dz);
			if (dist > radius) continue;

			float u = 1.f - (dist / radius);
			float fall = u * u * (3.f - 2.f * u); // smoothstep 페이드
			float& h = hm[i * W + j];

			switch (mode)
			{
			case 0: h += strength * fall; break; // 올림
			case 1: h -= strength * fall; break; // 내림
			case 2: // 스무드 (3x3 평균으로 끌어당김)
			{
				float avg = 0.f; int32 n = 0;
				for (int32 a = -1; a <= 1; ++a)
					for (int32 b = -1; b <= 1; ++b)
					{
						int32 ii = i + a, jj = j + b;
						if (ii < 0 || ii >= H || jj < 0 || jj >= W) continue;
						avg += hm[ii * W + jj]; ++n;
					}
				if (n > 0) avg /= n;
				float k = strength * fall * 0.5f; if (k > 1.f) k = 1.f;
				h = h + (avg - h) * k;
				break;
			}
			case 3: // 평탄화 (브러시 중심 높이로 끌어당김)
			{
				float k = strength * fall; if (k > 1.f) k = 1.f;
				h = h + (target - h) * k;
				break;
			}
			}
		}
	}

	_mesh->RebuildBoundsAndUploadVB();
	UploadHeightmap();
}

void Terrain::FlattenAll(float height)
{
	if (_mesh == nullptr)
		return;
	auto& hm = _mesh->HeightMapRef();
	std::fill(hm.begin(), hm.end(), height);
	_mesh->RebuildBoundsAndUploadVB();
	UploadHeightmap();
}
