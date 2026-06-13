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

	// 셰이더 이름 읽기. .mat 파일이 문자열로 저장했던 것이지만 FX11 제거, 이제는 Standard_HLSL만 사용
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

	// PBR 속성 저장 시도 (레거시 .mat 파일은 존재하지 않음 — 기본값 사용)
	float roughness = 0.f, metallic = 0.f;
	if (file->TryRead(roughness) && file->TryRead(metallic))
	{
		_desc.roughness = roughness;
		_desc.metallic  = metallic;
	}
}

// .mat 직렬화 — Load 와 1:1 대칭 (셰이더 헤더 / 텍스처 파일명 3종 / 색상 4종 / PBR 2종)
void Material::Save(const wstring& path)
{
	wstring fullPath = path;
	if (fullPath.size() < 4 || _wcsicmp(fullPath.substr(fullPath.size() - 4).c_str(), L".mat") != 0)
		fullPath += L".mat";

	// 텍스처는 파일명만 기록 (Load 가 .mat 의 부모 폴더 기준으로 다시 찾음)
	// GetName 은 비어 있을 수 있어 Texture::Load 가 기록하는 GetPath 사용
	auto texFileName = [](shared_ptr<Texture> tex) -> string
	{
		if (tex == nullptr)
			return "";
		return Utils::ToString(filesystem::path(tex->GetPath()).filename().wstring());
	};

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Write);

	file->Write<string>(string("01. Standard.fx")); // 포맷 호환 헤더 (로더는 문자열만 소비)
	file->Write<string>(texFileName(_diffuseMap));
	file->Write<string>(texFileName(_specularMap));
	file->Write<string>(texFileName(_normalMap));
	file->Write<Color>(_desc.ambient);
	file->Write<Color>(_desc.diffuse);
	file->Write<Color>(_desc.specular);
	file->Write<Color>(_desc.emissive);
	file->Write<float>(_desc.roughness);
	file->Write<float>(_desc.metallic);
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

		// 텍스처 샘플러 RenderStateManager에서 일괄 바인딩
		RENDER_STATES->BindAllSamplersPS();
	}
}

void Material::Refresh()
{
	// HlslShader SRV 정리
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
	material->_spotShadowMap = _spotShadowMap;
	material->_ssaoMap      = _ssaoMap;
	material->_renderQueue  = _renderQueue;   // 렌더 큐도 복사

	return material;
}
