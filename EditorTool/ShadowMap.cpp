#include "pch.h"
#include "ShadowMap.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Camera.h"
#include "Light.h"

ShadowMap::ShadowMap(uint32 width, uint32 height) : _width(width) , _height(height)
{
	_vp.Set(width, height , GAME->GetSceneDesc().x , GAME->GetSceneDesc().y);

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

	HRESULT hr;
	ComPtr<ID3D11Texture2D> depthMap;

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

	hr = DEVICE->CreateShaderResourceView(depthMap.Get(), &srvDesc, _shaderResourveView.GetAddressOf());
	CHECK(hr);
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Draw()
{
	_vp.RSSetViewport();

	ID3D11RenderTargetView* renderTargets[1] = { 0 };
	DCT->OMSetRenderTargets(1, renderTargets, _depthMapDSV.Get());
	DCT->ClearDepthStencilView(_depthMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	auto shader = RESOURCES->Get<Shader>(L"Shadow");
	auto camera = scene->GetMainCamera()->GetCamera();
	auto light = SCENE->GetCurrentScene()->GetLight()->GetLight();

	vector<shared_ptr<GameObject>> vecForward;

	for (auto& gameObject : gameObjects)
	{
		if (camera->IsCulled(gameObject->GetLayerIndex()))
			continue;

		if (gameObject->GetMeshRenderer() == nullptr
			&& gameObject->GetModelRenderer() == nullptr
			&& gameObject->GetModelAnimator() == nullptr)
			continue;

		vecForward.push_back(gameObject);
	}
	
	Matrix V = Light::S_MatView;
	Matrix P = Light::S_MatProjection;

	INSTANCING->Render(shader , V , P , light , vecForward);
}
