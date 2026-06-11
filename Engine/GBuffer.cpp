#include "pch.h"
#include "GBuffer.h"

void GBuffer::Init(uint32 width, uint32 height)
{
	_width = width;
	_height = height;

	DXGI_FORMAT formats[RT_COUNT] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,     // RT0: Albedo RGB + Metallic
		DXGI_FORMAT_R16G16B16A16_FLOAT, // RT1: World Normal XYZ + Roughness
		DXGI_FORMAT_R16G16B16A16_FLOAT, // RT2: World Position XYZ + mask
		DXGI_FORMAT_R11G11B10_FLOAT,    // RT3: Emissive RGB (HDR, 알파 불필요)
	};

	for (int i = 0; i < RT_COUNT; ++i)
	{
		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = formats[i];
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		CHECK(DEVICE->CreateTexture2D(&texDesc, nullptr, _textures[i].GetAddressOf()));

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = formats[i];
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		CHECK(DEVICE->CreateRenderTargetView(_textures[i].Get(), &rtvDesc, _rtvs[i].GetAddressOf()));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = formats[i];
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		CHECK(DEVICE->CreateShaderResourceView(_textures[i].Get(), &srvDesc, _srvs[i].GetAddressOf()));
	}

	D3D11_TEXTURE2D_DESC depthDesc{};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	CHECK(DEVICE->CreateTexture2D(&depthDesc, nullptr, _depthTexture.GetAddressOf()));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	CHECK(DEVICE->CreateDepthStencilView(_depthTexture.Get(), &dsvDesc, _dsv.GetAddressOf()));
}

void GBuffer::BindAsTarget()
{
	ID3D11RenderTargetView* rtvs[RT_COUNT];
	for (int i = 0; i < RT_COUNT; ++i)
		rtvs[i] = _rtvs[i].Get();

	DCT->OMSetRenderTargets(RT_COUNT, rtvs, _dsv.Get());

	// G-Buffer ?띿뒪泥섎뒗 (0,0) ?먯젏????ъ씠利덈줈 梨꾩썙???쒕떎.
	// (???덈룄??酉고룷?몃뒗 x/y ?ㅽ봽?뗭씠 ?덉쑝誘濡??ш린???먯젏 酉고룷?몃줈 ??뼱?대떎)
	D3D11_VIEWPORT vp{};
	vp.TopLeftX = 0.f;
	vp.TopLeftY = 0.f;
	vp.Width    = static_cast<float>(_width);
	vp.Height   = static_cast<float>(_height);
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	DCT->RSSetViewports(1, &vp);
}

void GBuffer::Clear()
{
	float clearBlack[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < RT_COUNT; ++i)
		DCT->ClearRenderTargetView(_rtvs[i].Get(), clearBlack);
	DCT->ClearDepthStencilView(_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void GBuffer::BindSRVsPS(uint32 startSlot) const
{
	// 라이팅 입력 3장만 연속 바인딩 (Emissive 는 호출부가 GetSRV(RT_EMISSIVE) 로 별도 슬롯에)
	ID3D11ShaderResourceView* srvs[RT_LIGHTING_COUNT];
	for (int i = 0; i < RT_LIGHTING_COUNT; ++i)
		srvs[i] = _srvs[i].Get();
	DCT->PSSetShaderResources(startSlot, RT_LIGHTING_COUNT, srvs);
}

void GBuffer::UnbindSRVsPS(uint32 startSlot) const
{
	ID3D11ShaderResourceView* nullSRV[RT_LIGHTING_COUNT] = {};
	DCT->PSSetShaderResources(startSlot, RT_LIGHTING_COUNT, nullSRV);
}
