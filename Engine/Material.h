#pragma once
#include "ResourceBase.h"

class Material : public ResourceBase
{
	using Super = ResourceBase;
public:
	Material();
	virtual ~Material();

	virtual void Load(const wstring& path) override;

	shared_ptr<HlslShader> GetHlslShader() { return _hlslShader; }

	MaterialDesc& GetMaterialDesc()   { return _desc; }
	shared_ptr<Texture> GetDiffuseMap()  { return _diffuseMap; }
	shared_ptr<Texture> GetNormalMap()   { return _normalMap; }
	shared_ptr<Texture> GetSpecularMap() { return _specularMap; }
	shared_ptr<Texture> GetShadowMap()   { return _shadowMap; }
	ComPtr<ID3D11ShaderResourceView> GetSsaoMap() { return _ssaoMap; }

	void SetHlslShader(shared_ptr<HlslShader> shader) { _hlslShader = shader; }

	void SetDiffuseMap(shared_ptr<Texture> t)  { _diffuseMap  = t; }
	void SetNormalMap(shared_ptr<Texture> t)   { _normalMap   = t; }
	void SetSpecularMap(shared_ptr<Texture> t) { _specularMap = t; }
	void SetShadowMap(shared_ptr<Texture> t)   { _shadowMap   = t; }
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv) { _ssaoMap = srv; }

	void Update();
	void Refresh();

	RenderQueue GetRenderQueue() const  { return _renderQueue; }
	void SetRenderQueue(RenderQueue q)  { _renderQueue = q; }

	shared_ptr<Material> Clone();

private:
	friend class MeshRenderer;

	MaterialDesc _desc;

	shared_ptr<HlslShader> _hlslShader;

	// 텍스처 맵 저장소
	shared_ptr<Texture> _diffuseMap;
	shared_ptr<Texture> _normalMap;
	shared_ptr<Texture> _specularMap;
	shared_ptr<Texture> _shadowMap;
	ComPtr<ID3D11ShaderResourceView> _ssaoMap;

	RenderQueue _renderQueue = RenderQueue::Opaque;
};
