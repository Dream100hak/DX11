#pragma once
#include "ResourceBase.h"

class Material : public ResourceBase
{
	using Super = ResourceBase;
public:
	Material();
	virtual ~Material();

	virtual void Load(const wstring& path) override;

	shared_ptr<Shader>     GetShader()     { return _shader; }
	shared_ptr<HlslShader> GetHlslShader() { return _hlslShader; }

	MaterialDesc& GetMaterialDesc()   { return _desc; }
	shared_ptr<Texture> GetDiffuseMap()  { return _diffuseMap; }
	shared_ptr<Texture> GetNormalMap()   { return _normalMap; }
	shared_ptr<Texture> GetSpecularMap() { return _specularMap; }
	shared_ptr<Texture> GetShadowMap()   { return _shadowMap; }
	ComPtr<ID3D11ShaderResourceView> GetSsaoMap() { return _ssaoMap; }

	void SetShader(shared_ptr<Shader> shader);
	void SetHlslShader(shared_ptr<HlslShader> shader) { _hlslShader = shader; }

	void SetDiffuseMap(shared_ptr<Texture> t)  { _diffuseMap  = t; }
	void SetNormalMap(shared_ptr<Texture> t)   { _normalMap   = t; }
	void SetSpecularMap(shared_ptr<Texture> t) { _specularMap = t; }
	void SetShadowMap(shared_ptr<Texture> t)   { _shadowMap   = t; }
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv) { _ssaoMap = srv; }

	void Update();
	void Refresh();

	shared_ptr<Material> Clone();

private:
	friend class MeshRenderer;

	MaterialDesc _desc;

	// FX11 경로 (기존)
	shared_ptr<Shader> _shader;
	ComPtr<ID3DX11EffectShaderResourceVariable> _diffuseEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _normalEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _specularEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _shadowMapEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _ssaoMapEffectBuffer;

	// HlslShader 경로 (신규)
	shared_ptr<HlslShader> _hlslShader;

	// 공용 텍스처
	shared_ptr<Texture> _diffuseMap;
	shared_ptr<Texture> _normalMap;
	shared_ptr<Texture> _specularMap;
	shared_ptr<Texture> _shadowMap;
	ComPtr<ID3D11ShaderResourceView> _ssaoMap;
};

