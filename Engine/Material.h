#pragma once
#include "ResourceBase.h"

class Material : public ResourceBase
{
	using Super = ResourceBase;
public:
	Material();
	virtual ~Material();

	virtual void Load(const wstring& path) override;

	shared_ptr<Shader> GetShader() { return _shader; }

	MaterialDesc& GetMaterialDesc() { return _desc; }
	shared_ptr<Texture> GetDiffuseMap() { return _diffuseMap; }
	shared_ptr<Texture> GetNormalMap() { return _normalMap; }
	shared_ptr<Texture> GetSpecularMap() { return _specularMap; }
	shared_ptr<Texture> GetShadowMap() { return _shadowMap; }
	ComPtr<ID3D11ShaderResourceView> GetSsaoMap() { return _ssaoMap; }

	void SetShader(shared_ptr<Shader> shader);
	void SetDiffuseMap(shared_ptr<Texture> diffuseMap) { _diffuseMap = diffuseMap;  /*_desc.useDiffuseMap = _diffuseMap ? 1 : 0;*/ }
	void SetNormalMap(shared_ptr<Texture> normalMap) { _normalMap = normalMap; }
	void SetSpecularMap(shared_ptr<Texture> specularMap) { _specularMap = specularMap; }
	void SetShadowMap(shared_ptr<Texture> shadowMap) { _shadowMap = shadowMap; }
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> ssaoMap) { _ssaoMap = ssaoMap; }

	void Update();
	void Refresh();

	shared_ptr<Material> Clone();


private:
	friend class MeshRenderer;

	MaterialDesc _desc;

	shared_ptr<Shader> _shader;
	shared_ptr<Texture> _diffuseMap;
	shared_ptr<Texture> _normalMap;
	shared_ptr<Texture> _specularMap;
	shared_ptr<Texture> _shadowMap; 
	ComPtr<ID3D11ShaderResourceView> _ssaoMap;

	ComPtr<ID3DX11EffectShaderResourceVariable> _diffuseEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _normalEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _specularEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _shadowMapEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _ssaoMapEffectBuffer;
};

