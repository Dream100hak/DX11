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

	//Shader — .mat 의 셰이더 문자열은 포맷 호환용으로만 읽음 (FX11 제거, 항상 Standard_HLSL)
	wstring shaderFile = Utils::ToWString(file->Read<string>());
	if (auto hlslShader = RESOURCES->Get<HlslShader>(L"Standard_HLSL"))
		SetHlslShader(hlslShader);

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

	// PBR 확장 필드 (구버전 .mat 에는 없음 — 기본값 유지)
	float roughness = 0.f, metallic = 0.f;
	if (file->TryRead(roughness) && file->TryRead(metallic))
	{
		_desc.roughness = roughness;
		_desc.metallic  = metallic;
	}
}

void Material::Update()
{
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

		// ���÷��� RenderStateManager���� ���� ������ ������ ���ε�
		RENDER_STATES->BindAllSamplersPS();
	}
}

void Material::Refresh()
{
	// HlslShader SRV Ŭ����
	if (_hlslShader)
	{
		for (UINT i = 0; i < 5; i++)
			_hlslShader->SetPSSRV(i, nullptr);
	}
}

std::shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();

	material->_hlslShader = _hlslShader;
	material->_desc         = _desc;
	material->_diffuseMap   = _diffuseMap;
	material->_normalMap    = _normalMap;
	material->_specularMap  = _specularMap;
	material->_shadowMap    = _shadowMap;
	material->_ssaoMap      = _ssaoMap;
	material->_renderQueue  = _renderQueue;   // �� RenderQueue ����

	return material;
}
