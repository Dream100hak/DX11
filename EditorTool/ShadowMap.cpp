#include "pch.h"
#include "ShadowMap.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Camera.h"
#include "Light.h"
#include "Terrain.h"

ShadowMap::ShadowMap(uint32 width, uint32 height) : _width(width) , _height(height)
{
	// 캐스케이드는 자기 텍스처 기준 원점 뷰포트 (씬 창 offset 넣으면 깊이가 어긋나게 기록됨)
	_vp.Set(static_cast<float>(width), static_cast<float>(height));

	// CASCADE_COUNT 슬라이스 깊이 텍스처 배열
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = _width;
	texDesc.Height = _height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = CASCADE_COUNT;
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

	// 슬라이스별 DSV — 캐스케이드 i 깊이 렌더 대상
	for (int32 i = 0; i < CASCADE_COUNT; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Flags = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;

		hr = DEVICE->CreateDepthStencilView(depthMap.Get(), &dsvDesc, _cascadeDSV[i].GetAddressOf());
		CHECK(hr);
	}

	// 배열 SRV — 디퍼드 라이팅에서 Texture2DArray 로 샘플
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = CASCADE_COUNT;

	hr = DEVICE->CreateShaderResourceView(depthMap.Get(), &srvDesc, _shaderResourveView.GetAddressOf());
	CHECK(hr);
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Draw()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	auto camera = scene->GetMainCamera()->GetCamera();

	// 라이트 없는 씬 (New Scene 직후) — 섀도우 패스 스킵 (null 역참조 크래시 방지)
	auto lightObj = scene->GetLight();
	if (lightObj == nullptr || lightObj->GetLight() == nullptr)
		return;
	auto light = lightObj->GetLight();

	vector<shared_ptr<GameObject>> vecForward;
	shared_ptr<Terrain> terrain = nullptr;

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetTerrain() != nullptr)
			terrain = gameObject->GetTerrain();

		if (camera->IsCulled(gameObject->GetLayerIndex()))
			continue;
		if (gameObject->GetRenderer() == nullptr)
			continue;
		if (gameObject->GetSkyBox())
			continue;

		vecForward.push_back(gameObject);
	}

	_vp.RSSetViewport();

	// 캐스케이드마다 라이트 시점 깊이를 자기 슬라이스에 렌더
	for (int32 i = 0; i < CASCADE_COUNT; ++i)
	{
		ID3D11RenderTargetView* renderTargets[1] = { nullptr };
		DCT->OMSetRenderTargets(1, renderTargets, _cascadeDSV[i].Get());
		DCT->ClearDepthStencilView(_cascadeDSV[i].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		Matrix V = Light::S_CascadeView[i];
		Matrix P = Light::S_CascadeProj[i];

		RenderContext ctx;
		ctx.tech = 0;
		ctx.view = V;
		ctx.proj = P;
		ctx.light = light;
		ctx.hlslOverride = nullptr;
		ctx.buffer = nullptr;
		ctx.lightArray = nullptr;
		ctx.shadowPass = true;

		INSTANCING->Render(ctx, vecForward);
		if (terrain)
			terrain->TerrainRendererNotPS(V, P);
	}
}
