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
	// 새 Texture 객체를 만듭니다.
	auto clonedTexture = std::make_shared<Texture>();

	// 기존 텍스처의 2D 텍스처 리소스를 가져옵니다.
	ComPtr<ID3D11Texture2D> srcTexture2D = GetTexture2D();

	// srcTexture2D의 설명을 얻어 새 텍스처를 만들기 위한 정보를 얻습니다.
	D3D11_TEXTURE2D_DESC textureDesc;
	srcTexture2D->GetDesc(&textureDesc);

	// 새 텍스처를 만듭니다.
	ComPtr<ID3D11Texture2D> newTexture2D;
	HRESULT hr = DEVICE.Get()->CreateTexture2D(&textureDesc, nullptr, newTexture2D.GetAddressOf());
	if (FAILED(hr))
	{
		// 오류 처리...
		return nullptr;
	}

	// 기존 텍스처에서 새 텍스처로 내용을 복사합니다.
	DCT->CopyResource(newTexture2D.Get(), srcTexture2D.Get());

	// 새 텍스처로부터 새 ShaderResourceView를 만듭니다.
	ComPtr<ID3D11ShaderResourceView> newSRV;
	hr = DEVICE.Get()->CreateShaderResourceView(newTexture2D.Get(), nullptr, newSRV.GetAddressOf());
	if (FAILED(hr))
	{
		// 오류 처리...
		return nullptr;
	}

	// 새 ShaderResourceView를 clonedTexture에 설정합니다.
	clonedTexture->SetSRV(newSRV);

	// 기타 정보를 복사합니다.
	clonedTexture->_size = _size;
	// 여기에 추가적으로 필요한 정보를 복사합니다.
	// clonedTexture->_img = ...;  // ScratchImage를 복제하는 것은 조금 더 복잡할 수 있습니다.

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
