#include "pch.h"
#include "Material.h"

Material::Material() : Super(ResourceType::Material)
{

}

Material::~Material()
{

}

void Material::SetShader(shared_ptr<Shader> shader)
{
	_shader = shader;

	_diffuseEffectBuffer = shader->GetSRV("DiffuseMap");
	_normalEffectBuffer = shader->GetSRV("NormalMap");
	_specularEffectBuffer = shader->GetSRV("SpecularMap");
	_cubeMapEffectBuffer = shader->GetSRV("CubeMap");
}

void Material::Update()
{
	if (_shader == nullptr)
		return;

	_shader->PushMaterialData(_desc);

	if (_diffuseMap)
		_diffuseEffectBuffer->SetResource(_diffuseMap->GetComPtr().Get());

	if (_normalMap)
		_normalEffectBuffer->SetResource(_normalMap->GetComPtr().Get());

	if (_specularMap)
		_specularEffectBuffer->SetResource(_specularMap->GetComPtr().Get());

	if (_cubeMap)
		_cubeMapEffectBuffer->SetResource(_cubeMap->GetComPtr().Get());
}

std::shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();

	material->_shader = _shader;
	material->_diffuseMap = _diffuseMap ? _diffuseMap->Clone() : nullptr;
	material->_normalMap = _normalMap ? _normalMap->Clone() : nullptr;
	material->_specularMap = _specularMap ? _specularMap->Clone() : nullptr;
	material->_cubeMap = _cubeMap ? _cubeMap->Clone() : nullptr;

	material->_desc = _desc;
	material->_diffuseMap = _diffuseMap;
	material->_normalMap = _normalMap;
	material->_specularMap = _specularMap;
	material->_diffuseEffectBuffer = _diffuseEffectBuffer;
	material->_normalEffectBuffer = _normalEffectBuffer;
	material->_specularEffectBuffer = _specularEffectBuffer;
	material->_cubeMapEffectBuffer = _cubeMapEffectBuffer;

	return material;
}
