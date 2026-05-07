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
	// FX11 Standard 셰이더 (기존 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Standard.fx");
	Add(L"Standard", shader);

	// HlslShader Standard 셰이더 (신규)
	HlslShaderDesc hlslDesc;
	hlslDesc.vsFile  = L"Standard_VS.hlsl";
	hlslDesc.psFile  = L"Standard_PS.hlsl";
	GetOrAddHlslShader(L"Standard_HLSL", hlslDesc);
}

void ResourceManager::CreateDefaultMaterial()
{
	auto shader = Get<Shader>(L"Standard");
	shared_ptr<Material> material = make_shared<Material>();
	material->SetShader(shader);
	MaterialDesc& desc = material->GetMaterialDesc();
	RESOURCES->Add(L"DefaultMaterial", material);
}

void ResourceManager::CreateShadowMapShader()
{
	// FX11 (기존 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"00. ShadowMap.fx");
	RESOURCES->Add(L"Shadow", shader);

	// HLSL (신규)
	HlslShaderDesc desc;
	desc.vsFile = L"ShadowMap_VS.hlsl";
	desc.psFile = L"ShadowMap_PS.hlsl";
	desc.vsEntry = "VS_Mesh";
	GetOrAddHlslShader(L"Shadow_HLSL", desc);
}

void ResourceManager::CreateOutlineShader()
{
	// FX11 (기존 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Outline.fx");
	RESOURCES->Add(L"Outline", shader);

	// HLSL (신규)
	HlslShaderDesc desc;
	desc.vsFile = L"Outline_VS.hlsl";
	desc.psFile = L"Outline_PS.hlsl";
	desc.vsEntry = "VS_MeshOutline";
	GetOrAddHlslShader(L"Outline_HLSL", desc);
}

void ResourceManager::CreateThumbnailShader()
{
	// FX11 (기존 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Thumbnail.fx");
	RESOURCES->Add(L"Thumbnail", shader);

	// HLSL (신규)
	HlslShaderDesc desc;
	desc.vsFile = L"Standard_VS.hlsl";   // VS 재활용
	desc.psFile = L"Thumbnail.hlsl";
	desc.psEntry = "PS_Solid";
	GetOrAddHlslShader(L"Thumbnail_HLSL", desc);
}

void ResourceManager::CreateSSAOShader()
{
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
	// FX11 (기존 유지)
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Terrain.fx");
	RESOURCES->Add(L"Terrain", shader);

	// HLSL (신규) ? Terrain.hlsl 은 단일 파일에 VS/HS/DS/PS 모두 포함
	// HlslShader 는 현재 HS/DS 미지원 → 추후 Tessellation 확장 시 추가
	// 현재는 FX11 유지
}
