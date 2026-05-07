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
	// Shader 버킷에 저장 (ResourceType::Shader 공유)
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
	hlslDesc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 진입점
	hlslDesc.psEntry = "PS_Main";
	GetOrAddHlslShader(L"Standard_HLSL", hlslDesc);

	// 레거시 FX11 Standard (Terrain 등 미이전 컴포넌트에서 참조할 수 있으므로 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Standard.fx");
	Add(L"Standard", shader);
}

void ResourceManager::CreateDefaultMaterial()
{
	// HlslShader 기반 기본 머티리얼
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
	desc.psEntry = "PS_AlphaClip";  // ShadowMap_PS.hlsl 의 진입점
	GetOrAddHlslShader(L"Shadow_HLSL", desc);

	// 레거시 FX11 (Terrain 섀도우 등)
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

	// 레거시 FX11
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Outline.fx");
	RESOURCES->Add(L"Outline", shader);
}

void ResourceManager::CreateThumbnailShader()
{
	// HLSL
	HlslShaderDesc desc;
	desc.vsFile  = L"Standard_VS.hlsl";
	desc.psFile  = L"Thumbnail.hlsl";
	desc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl 의 진입점
	desc.psEntry = "PS_Solid";
	GetOrAddHlslShader(L"Thumbnail_HLSL", desc);

	// 레거시 FX11
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Thumbnail.fx");
	RESOURCES->Add(L"Thumbnail", shader);
}

void ResourceManager::CreateSSAOShader()
{
	// SSAO는 아직 FX11 유지 (HLSL 미이전)
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

void ResourceManager::CreateTerrainShader()
{
	// Terrain은 Tessellation(HS/DS) 때문에 FX11 유지
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Terrain.fx");
	RESOURCES->Add(L"Terrain", shader);
}
