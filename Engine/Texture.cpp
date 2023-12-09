#include "pch.h"
#include "Texture.h"
#include <filesystem>

Texture::Texture() : Super(ResourceType::Texture)
{

}

Texture::~Texture()
{

}

std::shared_ptr<Texture> Texture::Clone()
{
	auto clonedTexture = std::make_shared<Texture>();

	ComPtr<ID3D11Texture2D> srcTexture2D = GetTexture2D();

	D3D11_TEXTURE2D_DESC textureDesc;
	srcTexture2D->GetDesc(&textureDesc);

	ComPtr<ID3D11Texture2D> newTexture2D;
	HRESULT hr = DEVICE.Get()->CreateTexture2D(&textureDesc, nullptr, newTexture2D.GetAddressOf());
	if (FAILED(hr))
	{
		// 오류 처리...
		return nullptr;
	}

	DCT->CopyResource(newTexture2D.Get(), srcTexture2D.Get());

	ComPtr<ID3D11ShaderResourceView> newSRV;
	hr = DEVICE.Get()->CreateShaderResourceView(newTexture2D.Get(), nullptr, newSRV.GetAddressOf());
	if (FAILED(hr))
		return nullptr;

	clonedTexture->SetSRV(newSRV);
	clonedTexture->_size = _size;

	return clonedTexture;
}

void Texture::Load(const wstring& path)
{
	wstring ext = std::filesystem::path(path).extension();

	DirectX::TexMetadata md;
	
	HRESULT hr; 

	if (ext == L".dds" || ext == L".DDS")
		hr = ::LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, &md, _img);
	else if (ext == L".tga" || ext == L".TGA")
		hr = ::LoadFromTGAFile(path.c_str(), &md, _img);
	else // png, jpg, jpeg, bmp
		hr = ::LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, &md, _img);

	hr = ::CreateShaderResourceView(DEVICE.Get(), _img.GetImages(), _img.GetImageCount(), md, _shaderResourveView.GetAddressOf());
	CHECK(hr);

	_size.x = md.width;
	_size.y = md.height;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture::GetTexture2D()
{
	ComPtr<ID3D11Texture2D> texture;
	_shaderResourveView->GetResource((ID3D11Resource**)texture.GetAddressOf());
	return texture;
}
