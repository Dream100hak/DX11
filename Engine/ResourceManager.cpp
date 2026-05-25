#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
#include "HlslShader.h"
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
	// HLSL
	HlslShaderDesc desc;
	desc.vsFile  = L"ShadowMap_VS.hlsl";
	desc.psFile  = L"ShadowMap_PS.hlsl";
	desc.vsEntry = "VS_Mesh";
	desc.psEntry = "PS_AlphaClip";  // ShadowMap_PS.hlsl 의 엔트리포인트
	GetOrAddHlslShader(L"Shadow_HLSL", desc);

	// 임시 FX11 (Terrain 그림자 용)
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

	// 임시 FX11
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Outline.fx");
	RESOURCES->Add(L"Outline", shader);
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
	// SSAO는 현재 FX11 유지 (HLSL 미지원)
	{
		shared_ptr<Shader> shader = make_shared<Shader>(L"00. Ssao.fx");
		RESOURCES->Add(L"Ssao", shader);
	}
	{
		shared_ptr<Shader> shader = make_shared<Shader>(L"00. SsaoNormalDepth.fx");
		RESOURCES->Add(L"SsaoNormalDepth", shader);
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
