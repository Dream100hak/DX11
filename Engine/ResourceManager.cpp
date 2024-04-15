#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
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
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Standard.fx");
	Add(L"Standard" , shader);
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
	shared_ptr<Shader> shader = make_shared<Shader>(L"00. ShadowMap.fx");
	RESOURCES->Add(L"Shadow", shader);
}

void ResourceManager::CreateOutlineShader()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Outline.fx");
	RESOURCES->Add(L"Outline", shader);
}

void ResourceManager::CreateThumbnailShader()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"01. Thumbnail.fx");
	RESOURCES->Add(L"Thumbnail", shader);
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
