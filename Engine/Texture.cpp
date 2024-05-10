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

	ComPtr<ID3D11Texture2D> srcTexture2D = GetTexture2D().Get(); // 수정된 부분

	D3D11_TEXTURE2D_DESC textureDesc;
	srcTexture2D->GetDesc(&textureDesc);

	// Shader Resource View 생성을 위해 BindFlags 수정
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

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
	{
		// 오류 처리...
		return nullptr;
	}

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

	_path = path;
}



Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture::GetTexture2D()
{
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	_shaderResourveView->GetResource(&resource);
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	resource.As(&texture);
	return texture;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture::CreateTexture2DArraySRV(std::vector<std::wstring>& filenames)
{
	//
// Load the texture elements individually from file.  These textures
// won't be used by the GPU (0 bind flags), they are just used to 
// load the image data from file.  We use the STAGING usage so the
// CPU can read the resource.
//

	uint32 size = filenames.size();

	std::vector<ComPtr<ID3D11Texture2D>> srcTex(size);

	for (uint32 i = 0; i < size; ++i)
	{
		DirectX::TexMetadata md = {};

		DirectX::ScratchImage img;
		HRESULT hr = ::LoadFromDDSFile(filenames[i].c_str(), DDS_FLAGS_NONE, &md, img);
		CHECK(hr);

		hr = ::CreateTextureEx(DEVICE.Get(), img.GetImages(), img.GetImageCount(), md,
			D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, false, (ID3D11Resource**)srcTex[i].GetAddressOf());

		CHECK(hr);
	}

	//
	// Create the texture array.  Each element in the texture 
	// array has the same format/dimensions.
	//

	D3D11_TEXTURE2D_DESC texElementDesc;
	srcTex[0]->GetDesc(&texElementDesc);
	//D3D11_TEXTURE2D_DESC texElementDesc1;
	//srcTex[1]->GetDesc(&texElementDesc1);
	//D3D11_TEXTURE2D_DESC texElementDesc2;
	//srcTex[2]->GetDesc(&texElementDesc2);
	//D3D11_TEXTURE2D_DESC texElementDesc3;
	//srcTex[3]->GetDesc(&texElementDesc3);


	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width = texElementDesc.Width;
	texArrayDesc.Height = texElementDesc.Height;
	texArrayDesc.MipLevels = texElementDesc.MipLevels;
	texArrayDesc.ArraySize = size;
	texArrayDesc.Format = texElementDesc.Format;
	texArrayDesc.SampleDesc.Count = 1;
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texArrayDesc.CPUAccessFlags = 0;
	texArrayDesc.MiscFlags = 0;

	HRESULT hr;

	ComPtr<ID3D11Texture2D> texArray;
	hr = DEVICE->CreateTexture2D(&texArrayDesc, 0, texArray.GetAddressOf());
	CHECK(hr);

	//
	// Copy individual texture elements into texture array.
	//

	// for each texture element...
	for (uint32 texElement = 0; texElement < size; ++texElement)
	{
		// for each mipmap level...
		for (uint32 mipLevel = 0; mipLevel < texElementDesc.MipLevels; ++mipLevel)
		{
			D3D11_MAPPED_SUBRESOURCE mappedTex2D;
			hr = DCT->Map(srcTex[texElement].Get(), mipLevel, D3D11_MAP_READ, 0, &mappedTex2D);
			CHECK(hr);

			DCT->UpdateSubresource(texArray.Get(),
				::D3D11CalcSubresource(mipLevel, texElement, texElementDesc.MipLevels),
				0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);

			DCT->Unmap(srcTex[texElement].Get(), mipLevel);
		}
	}

	//
	// Create a resource view to the texture array.
	//

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	ZeroMemory(&viewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	viewDesc.Format = texArrayDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	viewDesc.Texture2DArray.MostDetailedMip = 0;
	viewDesc.Texture2DArray.MipLevels = texArrayDesc.MipLevels;
	viewDesc.Texture2DArray.FirstArraySlice = 0;
	viewDesc.Texture2DArray.ArraySize = size;

	ComPtr<ID3D11ShaderResourceView> texArraySRV;
	hr = DEVICE->CreateShaderResourceView(texArray.Get(), &viewDesc, texArraySRV.GetAddressOf());
	CHECK(hr);

	return texArraySRV;
}
