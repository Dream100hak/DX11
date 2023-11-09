#include "pch.h"
#include "ShadowMap.h"


ShadowMap::ShadowMap(uint32 width, uint32 height) : _width(width) , _height(height)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = _width;
	texDesc.Height = _height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthMap;
	HRESULT hr;

	hr = DEVICE->CreateTexture2D(&texDesc, 0, depthMap.GetAddressOf());
	CHECK(hr);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	
	hr = DEVICE->CreateDepthStencilView(depthMap.Get(), &dsvDesc, _depthMapDSV.GetAddressOf());
	CHECK(hr);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	
	hr = DEVICE->CreateShaderResourceView(depthMap.Get(), &srvDesc, GetComPtr().GetAddressOf());
	CHECK(hr);
}

ShadowMap::~ShadowMap()
{

}

void ShadowMap::BindDsvAndSetNullRenderTarget()
{
	ID3D11RenderTargetView* renderTargets[1] = { 0 };
	DCT->OMSetRenderTargets(1, renderTargets, _depthMapDSV.Get());
	DCT->ClearDepthStencilView(_depthMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void ShadowMap::DrawSceneToShadowMap()
{
	
}

void ShadowMap::BuildShadowTransform()
{
	//// Only the first "main" light casts a shadow.
	//XMVECTOR lightDir = ::XMLoadFloat3(&_dirLights[0].Direction);
	//XMVECTOR lightPos = -2.0f * _sceneBounds.Radius * lightDir;
	//XMVECTOR targetPos = ::XMLoadFloat3(&_sceneBounds.Center);
	//XMVECTOR up = ::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//XMMATRIX V = ::XMMatrixLookAtLH(lightPos, targetPos, up);

	//// Transform bounding sphere to light space.
	//XMFLOAT3 sphereCenterLS;
	//::XMStoreFloat3(&sphereCenterLS, ::XMVector3TransformCoord(targetPos, V));

	//// Ortho frustum in light space encloses scene.
	//float l = sphereCenterLS.x - _sceneBounds.Radius;
	//float b = sphereCenterLS.y - _sceneBounds.Radius;
	//float n = sphereCenterLS.z - _sceneBounds.Radius;
	//float r = sphereCenterLS.x + _sceneBounds.Radius;
	//float t = sphereCenterLS.y + _sceneBounds.Radius;
	//float f = sphereCenterLS.z + _sceneBounds.Radius;
	//XMMATRIX P = ::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	//// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	//XMMATRIX T(
	//	0.5f, 0.0f, 0.0f, 0.0f,
	//	0.0f, -0.5f, 0.0f, 0.0f,
	//	0.0f, 0.0f, 1.0f, 0.0f,
	//	0.5f, 0.5f, 0.0f, 1.0f);

	//XMMATRIX S = V * P * T;

	//::XMStoreFloat4x4(&_lightView, V);
	//::XMStoreFloat4x4(&_lightProj, P);
	//::XMStoreFloat4x4(&_shadowTransform, S);
}

