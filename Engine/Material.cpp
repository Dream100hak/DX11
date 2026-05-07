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
	SetName(fullPath);
	auto fileSystemPath = filesystem::path(fullPath);
	auto parentPath = fileSystemPath.parent_path();

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Read);

	//Shader
	wstring shaderFile = Utils::ToWString(file->Read<string>());
	shared_ptr<Shader> shader = RESOURCES->Get<Shader>(shaderFile);

	if (shader == nullptr)
	{
		shader = make_shared<Shader>(shaderFile);
	}
	SetShader(shader);

	wstring diffuseStr = Utils::ToWString(file->Read<string>());
	if (diffuseStr.length() > 0)
	{
		auto texture = RESOURCES->GetOrAddTexture(diffuseStr, (parentPath / diffuseStr).wstring());
		_diffuseMap = texture;
	}
	wstring specularStr = Utils::ToWString(file->Read<string>());
	if (specularStr.length() > 0)
	{
		auto texture = RESOURCES->GetOrAddTexture(specularStr, (parentPath / specularStr).wstring());
		_specularMap = texture;
	}
	wstring normalStr = Utils::ToWString(file->Read<string>());
	if (normalStr.length() > 0)
	{
		auto texture = RESOURCES->GetOrAddTexture(normalStr, (parentPath / normalStr).wstring());
		_normalMap = texture;
	}
	_desc.ambient = file->Read<Color>();
	_desc.diffuse = file->Read<Color>();
	_desc.specular = file->Read<Color>();
	_desc.emissive = file->Read<Color>();
}

void Material::SetShader(shared_ptr<Shader> shader)
{
	_shader = shader;
}

void Material::Update()
{
	// ── FX11 경로 (Terrain 등 레거시) ─────────────────────────
	if (_shader)
	{
		_desc.useTexture = _diffuseMap ? 1 : 0;
		_shader->PushMaterialData(_desc);
	}

	// ── HlslShader 경로 (신규) ────────────────────────────────
	if (_hlslShader)
	{
		_desc.useTexture = _diffuseMap ? 1 : 0;
		_hlslShader->PushMaterialData(_desc);

		auto bindSRV = [&](UINT slot, shared_ptr<Texture> tex)
		{
			ID3D11ShaderResourceView* srv = tex ? tex->GetComPtr().Get() : nullptr;
			_hlslShader->SetPSSRV(slot, srv);
		};
		bindSRV(0, _diffuseMap);
		bindSRV(1, _specularMap);
		bindSRV(2, _normalMap);
		bindSRV(3, _shadowMap);

		ID3D11ShaderResourceView* ssaoSrv = _ssaoMap.Get();
		_hlslShader->SetPSSRV(4, ssaoSrv);
	}
}

void Material::Refresh()
{
	// HlslShader SRV 클리어
	if (_hlslShader)
	{
		for (UINT i = 0; i < 5; i++)
			_hlslShader->SetPSSRV(i, nullptr);
	}
}

std::shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();

	material->_shader     = _shader;
	material->_hlslShader = _hlslShader;

	material->_desc         = _desc;
	material->_diffuseMap   = _diffuseMap;
	material->_normalMap    = _normalMap;
	material->_specularMap  = _specularMap;
	material->_shadowMap    = _shadowMap;
	material->_ssaoMap      = _ssaoMap;

	return material;
}
