#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
#include <filesystem>

void ResourceManager::Init()
{
	CreateDefaultMesh();
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
	shared_ptr<Shader> shader = make_shared<Shader>(L"Standard.fx");
	Add(L"Standard" , shader);
}

void ResourceManager::CreateDefaultMaterial()
{
	//auto shader = Get<Shader>(L"Standard");
	//auto shaderBuffer = make_shared<ShaderBuffer>(shader);

	//shared_ptr<Material> material = make_shared<Material>();
	//material->SetShader(shaderBuffer);
	//MaterialDesc& desc = material->GetMaterialDesc();
	//desc.ambient = Vec4(1.f);
	//desc.diffuse = Vec4(1.f);
	//desc.specular = Vec4(1.f);
	//RESOURCES->Add(L"DefaultMaterial", material);
}
