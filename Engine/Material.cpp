#include "pch.h"
#include "Material.h"
#include <filesystem>
#include "Utils.h"
#include "FileUtils.h"

Material::Material() : Super(ResourceType::Material)
{
}

Material::~Material()
{
}

void Material::Load(const wstring& path)
{
	wstring fullPath = path + L".mat";

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Read);

	wstring shaderFile = Utils::ToWString(file->Read<string>());
	shared_ptr<Shader> shader = RESOURCES->Get<Shader>(shaderFile);

	if (shader == nullptr)
	{
		shader = make_shared<Shader>(shaderFile); 
	}
	SetShader(shader);

	_desc.ambient = file->Read<Color>();
	_desc.diffuse = file->Read<Color>();
	_desc.specular = file->Read<Color>();
	_desc.emissive = file->Read<Color>();
}

void Material::SetShader(shared_ptr<Shader> shader)
{
	_shader = shader;

	_diffuseEffectBuffer = shader->GetSRV("DiffuseMap");
	_normalEffectBuffer = shader->GetSRV("NormalMap");
	_specularEffectBuffer = shader->GetSRV("SpecularMap");
	_shadowMapEffectBuffer = shader->GetSRV("ShadowMap");
	_ssaoMapEffectBuffer = shader->GetSRV("SsaoMap");
}

void Material::Update()
{
	if (_shader == nullptr)
		return;

	bool useTexture = 0;

	if (_diffuseMap)
	{
		_diffuseEffectBuffer->SetResource(_diffuseMap->GetComPtr().Get());
		useTexture = 1;
	}

	if(_normalMap)
		_normalEffectBuffer->SetResource(_normalMap->GetComPtr().Get());

	if (_specularMap)
		_specularEffectBuffer->SetResource(_specularMap->GetComPtr().Get());

	if (_shadowMap)
		_shadowMapEffectBuffer->SetResource(_shadowMap->GetComPtr().Get());

	if (_ssaoMap)
		_ssaoMapEffectBuffer->SetResource(_ssaoMap.Get());

	_desc.useTexture = useTexture;

	_shader->PushMaterialData(_desc);
}

void Material::Refresh()
{
	_diffuseEffectBuffer->SetResource(nullptr);
	_normalEffectBuffer->SetResource(nullptr);
	_specularEffectBuffer->SetResource(nullptr);
	_shadowMapEffectBuffer->SetResource(nullptr);
	_ssaoMapEffectBuffer->SetResource(nullptr);
}

std::shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();
//	shared_ptr<Shader> shader = make_shared<Shader>(_shader);

	material->_shader = _shader;
	material->_diffuseMap = _diffuseMap ? _diffuseMap->Clone() : nullptr;
	material->_normalMap = _normalMap ? _normalMap->Clone() : nullptr;
	material->_specularMap = _specularMap ? _specularMap->Clone() : nullptr;
	material->_shadowMap = _shadowMap ? _shadowMap->Clone() : nullptr;

	material->_desc = _desc;
	material->_diffuseMap = _diffuseMap;
	material->_normalMap = _normalMap;
	material->_specularMap = _specularMap;
	material->_shadowMap = _shadowMap;
	material->_diffuseEffectBuffer = _diffuseEffectBuffer;
	material->_normalEffectBuffer = _normalEffectBuffer;
	material->_specularEffectBuffer = _specularEffectBuffer;
	material->_shadowMapEffectBuffer = _shadowMapEffectBuffer;
	material->_ssaoMapEffectBuffer = _ssaoMapEffectBuffer;

	Refresh();

	return material;
}
