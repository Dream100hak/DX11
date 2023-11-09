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
	// �� Texture ��ü�� ����ϴ�.
	auto clonedTexture = std::make_shared<Texture>();

	// ���� �ؽ�ó�� 2D �ؽ�ó ���ҽ��� �����ɴϴ�.
	ComPtr<ID3D11Texture2D> srcTexture2D = GetTexture2D();

	// srcTexture2D�� ������ ��� �� �ؽ�ó�� ����� ���� ������ ����ϴ�.
	D3D11_TEXTURE2D_DESC textureDesc;
	srcTexture2D->GetDesc(&textureDesc);

	// �� �ؽ�ó�� ����ϴ�.
	ComPtr<ID3D11Texture2D> newTexture2D;
	HRESULT hr = DEVICE.Get()->CreateTexture2D(&textureDesc, nullptr, newTexture2D.GetAddressOf());
	if (FAILED(hr))
	{
		// ���� ó��...
		return nullptr;
	}

	// ���� �ؽ�ó���� �� �ؽ�ó�� ������ �����մϴ�.
	DCT->CopyResource(newTexture2D.Get(), srcTexture2D.Get());

	// �� �ؽ�ó�κ��� �� ShaderResourceView�� ����ϴ�.
	ComPtr<ID3D11ShaderResourceView> newSRV;
	hr = DEVICE.Get()->CreateShaderResourceView(newTexture2D.Get(), nullptr, newSRV.GetAddressOf());
	if (FAILED(hr))
	{
		// ���� ó��...
		return nullptr;
	}

	// �� ShaderResourceView�� clonedTexture�� �����մϴ�.
	clonedTexture->SetSRV(newSRV);

	// ��Ÿ ������ �����մϴ�.
	clonedTexture->_size = _size;
	// ���⿡ �߰������� �ʿ��� ������ �����մϴ�.
	// clonedTexture->_img = ...;  // ScratchImage�� �����ϴ� ���� ���� �� ������ �� �ֽ��ϴ�.

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
