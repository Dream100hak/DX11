#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
#include "HlslShader.h"
#include "RenderStateManager.h"
#include <filesystem>

void ResourceManager::Init()
{
	CreateDefaultMesh();
	CreateDefaultShader();
	CreateDefaultMaterial();

	CreateShadowMapShader();
	CreateOutlineShader();
	CreateThumbnailShader();
	CreateSSAOShader();
	CreateTerrainShader();
	CreateDeferredShaders();
	CreateEditorMiscShaders();
	CreateParticleShaders();
}

void ResourceManager::CreateParticleShaders()
{
	// 파티클 SO 정점 레이아웃: POS.xyz / VELOCITY.xyz / SIZE.xy / AGE.x / TYPE.x (40 bytes)
	// FX ConstructGSWithSO("POS.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; TYPE.x") 대체
	const vector<D3D11_SO_DECLARATION_ENTRY> soEntries =
	{
		{ 0, "POS",      0, 0, 3, 0 },
		{ 0, "VELOCITY", 0, 0, 3, 0 },
		{ 0, "SIZE",     0, 0, 2, 0 },
		{ 0, "AGE",      0, 0, 1, 0 },
		{ 0, "TYPE",     0, 0, 1, 0 },
	};
	const uint32 soStride = sizeof(float) * 9 + sizeof(uint32); // 40

	// ── Fire ──
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Fire.hlsl";
		desc.gsFile  = L"Fire.hlsl";
		desc.vsEntry = "VS_StreamOut";
		desc.gsEntry = "GS_StreamOut";
		desc.soEntries = soEntries;
		desc.soStride  = soStride;
		auto s = GetOrAddHlslShader(L"FireSO_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth));
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Fire.hlsl";
		desc.gsFile  = L"Fire.hlsl";
		desc.psFile  = L"Fire.hlsl";
		desc.vsEntry = "VS_Draw";
		desc.gsEntry = "GS_Draw";
		desc.psEntry = "PS_Draw";
		auto s = GetOrAddHlslShader(L"FireDraw_HLSL", desc);
		if (s)
		{
			s->SetBlendState(RENDER_STATES->GetBS(BlendStateType::AdditiveSrcAlpha));
			s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));
		}
	}

	// ── Rain ──
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Rain.hlsl";
		desc.gsFile  = L"Rain.hlsl";
		desc.vsEntry = "VS_StreamOut";
		desc.gsEntry = "GS_StreamOut";
		desc.soEntries = soEntries;
		desc.soStride  = soStride;
		auto s = GetOrAddHlslShader(L"RainSO_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth));
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Rain.hlsl";
		desc.gsFile  = L"Rain.hlsl";
		desc.psFile  = L"Rain.hlsl";
		desc.vsEntry = "VS_Draw";
		desc.gsEntry = "GS_Draw";
		desc.psEntry = "PS_Draw";
		auto s = GetOrAddHlslShader(L"RainDraw_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));
	}
}

std::shared_ptr<Texture> ResourceManager::GetOrAddTexture(const wstring& key, const wstring& path)
{
	shared_ptr<Texture> texture = Get<Texture>(key);

	if (filesystem::exists(filesystem::path(path)) == false)
		return nullptr;

	texture = Load<Texture>(key, path);

	if (texture == nullptr)
	{
		texture = make_shared<Texture>();
		texture->Load(path);
		Add(key, texture);
	}

	return texture;
}

shared_ptr<HlslShader> ResourceManager::GetOrAddHlslShader(const wstring& key, const HlslShaderDesc& desc)
{
	// Shader 버킷에 조회 (ResourceType::Shader 기준)
	auto& bucket = _resources[static_cast<uint8>(ResourceType::Shader)];
	auto it = bucket.find(key);
	if (it != bucket.end())
		return static_pointer_cast<HlslShader>(it->second);

	auto shader = make_shared<HlslShader>();
	shader->SetName(key);
	shader->Create(desc);
	bucket[key] = shader;
	return shader;
}

void ResourceManager::CreateDefaultMesh()
{
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateQuad();
		Add(L"Quad", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateCube();
		Add(L"Cube", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateSphere();
		Add(L"Sphere", mesh);
	}
}

void ResourceManager::CreateDefaultShader()
{
	// HlslShader Standard 셰이더 (FX 01. Standard.fx 는 제거됨)
	HlslShaderDesc hlslDesc;
	hlslDesc.vsFile  = L"Standard_VS.hlsl";
	hlslDesc.psFile  = L"Standard_PS.hlsl";
	hlslDesc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 엔트리포인트
	hlslDesc.psEntry = "PS_Main";
	GetOrAddHlslShader(L"Standard_HLSL", hlslDesc);
}

void ResourceManager::CreateDefaultMaterial()
{
	// HlslShader 용 기본 머티리얼
	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(Get<HlslShader>(L"Standard_HLSL"));
	MaterialDesc& desc = material->GetMaterialDesc();
	RESOURCES->Add(L"DefaultMaterial", material);
}

void ResourceManager::CreateShadowMapShader()
{
	// Depth-only 그림자 패스 (HLSL). Standard_VS 의 스키닝/본/트윈 로직을 그대로 재사용하고
	// light VP 는 PushGlobalData(lightV, lightP) 로 b0 VP 에 들어간다. PS 는 알파클립만 수행.
	auto shadowRS = RENDER_STATES->GetRS(RasterizerStateType::ShadowDepth);
	// ── Mesh ──
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"Shadow_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
	// ── 정적 모델 ──
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"ShadowModel_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
	// ── 애니메이션 모델 ──
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"ShadowAnim_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
}

void ResourceManager::CreateOutlineShader()
{
	// HLSL
	HlslShaderDesc desc;
	desc.vsFile  = L"Outline_VS.hlsl";
	desc.psFile  = L"Outline_PS.hlsl";
	desc.vsEntry = "VS_MeshOutline";
	GetOrAddHlslShader(L"Outline_HLSL", desc);
	// FX11 01. Outline.fx 는 소비처가 없어 제거됨 (Outline_HLSL 만 사용)
}

void ResourceManager::CreateThumbnailShader()
{
	// HLSL (FX 01. Thumbnail.fx 는 제거됨 — 프리뷰는 ModelPreview_HLSL/AnimPreview_HLSL)
	HlslShaderDesc desc;
	desc.vsFile  = L"Standard_VS.hlsl";
	desc.psFile  = L"Thumbnail.hlsl";
	desc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 엔트리포인트
	desc.psEntry = "PS_Solid";
	GetOrAddHlslShader(L"Thumbnail_HLSL", desc);
}

void ResourceManager::CreateSSAOShader()
{
	// SSAO normal-depth 패스 (모델 렌더) — HLSL. FX 00. SsaoNormalDepth.fx 의 모델 오버라이드 대체.
	// view-space normal + view-space depth 를 출력.
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepth_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepthModel_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepthAnim_HLSL", desc);
	}

	// SSAO compute (HLSL) — FX 00. Ssao.fx 대체. 샘플러는 Ssao 클래스에서 직접 바인딩.
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Ssao.hlsl";
		desc.psFile  = L"Ssao.hlsl";
		GetOrAddHlslShader(L"Ssao_HLSL", desc);
	}
	// SSAO blur (HLSL) — FX 00. SsaoBlur.fx 대체. 가로/세로는 cbuffer HorzBlur 로 분기.
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoBlur.hlsl";
		desc.psFile  = L"SsaoBlur.hlsl";
		GetOrAddHlslShader(L"SsaoBlur_HLSL", desc);
	}
}

void ResourceManager::CreateDeferredShaders()
{
	// G-Buffer fill (VS_Mesh + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBuffer_HLSL", desc);
	}

	// G-Buffer fill for static models (VS_Model + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBufferModel_HLSL", desc);
	}

	// G-Buffer fill for animated models (VS_Animation + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBufferAnim_HLSL", desc);
	}

	// Deferred Lighting (fullscreen triangle, no vertex buffer)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"DeferredLighting.hlsl";
		desc.psFile  = L"DeferredLighting.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"DeferredLighting_HLSL", desc);
	}

	// Tonemap (HDR sceneColor -> 백버퍼, ACES + 감마)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Tonemap.hlsl";
		desc.psFile  = L"Tonemap.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Tonemap_HLSL", desc);
	}

	// Pass Viewer (씬뷰 패스 시각화 — Albedo/Normal/Roughness/Metallic/Depth/SSAO/Shadow)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"PassViewer.hlsl";
		desc.psFile  = L"PassViewer.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"PassViewer_HLSL", desc);
	}

	// G-Buffer Debug View (4-quadrant split)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"GBufferDebug.hlsl";
		desc.psFile  = L"GBufferDebug.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"GBufferDebug_HLSL", desc);
	}
}

void ResourceManager::CreateEditorMiscShaders()
{
	// SceneGrid (라인 리스트, 알파블렌드 그리드) — FX11 01. SceneGrid.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"SceneGrid.hlsl";
		desc.psFile = L"SceneGrid.hlsl";
		GetOrAddHlslShader(L"SceneGrid_HLSL", desc);
	}

	// Collider (라인 박스, 정점 컬러) — FX11 01. Collider.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"Collider.hlsl";
		desc.psFile = L"Collider.hlsl";
		auto shader = GetOrAddHlslShader(L"Collider_HLSL", desc);
		if (shader)
		{
			shader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::Default));
			shader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullNone));
		}
	}

	// CubeMap 스카이박스 (SkyCubeMap, 에디터) — FX11 01. CubeMap.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"CubeMap.hlsl";
		desc.psFile = L"CubeMap.hlsl";
		auto shader = GetOrAddHlslShader(L"CubeMap_HLSL", desc);
		if (shader)
		{
			shader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::SkyBoxDepth));
			shader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::FrontCounterCW));
		}
	}

	// 모델 프리뷰/썸네일 (정적) — VS_Model + PS_PreviewLit. FX Standard/Thumbnail 대체
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"Thumbnail.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_PreviewLit";
		GetOrAddHlslShader(L"ModelPreview_HLSL", desc);
	}

	// 모델 프리뷰/썸네일 (애니메이션) — VS_Animation + PS_PreviewLit
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"Thumbnail.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_PreviewLit";
		GetOrAddHlslShader(L"AnimPreview_HLSL", desc);
	}
}

void ResourceManager::CreateTerrainShader()
{
	// Terrain HLSL (VS + HS + DS + PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Terrain_HLSL", desc);
	}

	// Terrain Shadow HLSL (VS + HS + DS only, depth-only pass)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		GetOrAddHlslShader(L"Terrain_Shadow_HLSL", desc);
	}

	// Terrain GBuffer HLSL (VS + HS + DS + PS_GBuffer) — 디퍼드 GBuffer fill
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"Terrain_GBuffer_HLSL", desc);
	}

	// Terrain SSAO normal-depth HLSL (VS + HS + DS + PS_NormalDepth)
	// SSAO 패스에서 터레인이 view-space normal+depth 를 기록하도록 (depth 만 쓰던 갭 해소)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_NormalDepth";
		GetOrAddHlslShader(L"Terrain_NormalDepth_HLSL", desc);
	}
}
