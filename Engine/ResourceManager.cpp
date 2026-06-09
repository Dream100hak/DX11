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
	// HlslShader Standard 셰이더
	HlslShaderDesc hlslDesc;
	hlslDesc.vsFile  = L"Standard_VS.hlsl";
	hlslDesc.psFile  = L"Standard_PS.hlsl";
	hlslDesc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 엔트리포인트
	hlslDesc.psEntry = "PS_Main";
	GetOrAddHlslShader(L"Standard_HLSL", hlslDesc);

	// 임시 FX11 Standard (Terrain 등 컴포넌트가 참조하므로 잠시 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Standard.fx");
	Add(L"Standard", shader);
}

void ResourceManager::CreateDefaultMaterial()
{
	// HlslShader 용 기본 머티리얼
	auto hlslShader = Get<HlslShader>(L"Standard_HLSL");
	shared_ptr<Material> material = make_shared<Material>();
	if (hlslShader)
		material->SetHlslShader(hlslShader);
	else
	{
		auto shader = Get<Shader>(L"Standard");
		material->SetShader(shader);
	}
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

	// 임시 FX11 (Terrain 그림자 용 — Terrain_Shadow_HLSL 로 대체 진행 중)
	shared_ptr<Shader> shader = make_shared<Shader>(L"00. ShadowMap.fx");
	RESOURCES->Add(L"Shadow", shader);
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
	// HLSL
	HlslShaderDesc desc;
	desc.vsFile  = L"Standard_VS.hlsl";
	desc.psFile  = L"Thumbnail.hlsl";
	desc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 엔트리포인트
	desc.psEntry = "PS_Solid";
	GetOrAddHlslShader(L"Thumbnail_HLSL", desc);

	// 임시 FX11
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Thumbnail.fx");
	RESOURCES->Add(L"Thumbnail", shader);
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

	// SSAO compute / blur 는 아직 FX11 유지 (별도 단계에서 HLSL 이전 예정)
	{
		shared_ptr<Shader> shader = make_shared<Shader>(L"00. Ssao.fx");
		RESOURCES->Add(L"Ssao", shader);
	}
	{
		shared_ptr<Shader> shader = make_shared<Shader>(L"00. SsaoBlur.fx");
		RESOURCES->Add(L"SsaoBlur", shader);
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
}
