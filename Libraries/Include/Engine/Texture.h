#pragma once
#include "ResourceBase.h"

class Texture : public ResourceBase
{
	using Super = ResourceBase;
public:
	Texture();
	virtual ~Texture();

	shared_ptr<Texture> Clone();


	virtual void Load(const wstring& path) override;
	void CreateRandomTexture1DSRV();
	void CreateTexture2DArraySRV(std::vector<std::wstring>& filenames);

	void							 SetSRV(ComPtr<ID3D11ShaderResourceView> srv) { _shaderResourveView = srv; }
	ComPtr<ID3D11ShaderResourceView> GetComPtr() { return _shaderResourveView; }
	ComPtr<ID3D11Texture2D>			 GetTexture2D();
	Vec2							 GetSize() { return _size; }
	const DirectX::ScratchImage&	 GetInfo() { return _img; }

protected :

	ComPtr<ID3D11ShaderResourceView> _shaderResourveView;
	Vec2 _size = { 0.f, 0.f };
	DirectX::ScratchImage _img = {};
};

