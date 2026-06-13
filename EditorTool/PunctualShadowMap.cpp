#include "pch.h"
#include "PunctualShadowMap.h"
#include "Camera.h"
#include "Light.h"
#include "Terrain.h"

PunctualShadowMap::PunctualShadowMap(uint32 size) : _size(size)
{
	_vp.Set(static_cast<float>(size), static_cast<float>(size));

	// 스팟 슬롯 깊이 텍스처 배열
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = _size;
	texDesc.Height = _size;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = MAX_PUNCTUAL_SHADOWS;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> depthTex;
	CHECK(DEVICE->CreateTexture2D(&texDesc, 0, depthTex.GetAddressOf()));

	for (int32 i = 0; i < MAX_PUNCTUAL_SHADOWS; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		CHECK(DEVICE->CreateDepthStencilView(depthTex.Get(), &dsvDesc, _slotDSV[i].GetAddressOf()));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = MAX_PUNCTUAL_SHADOWS;
	CHECK(DEVICE->CreateShaderResourceView(depthTex.Get(), &srvDesc, _shaderResourveView.GetAddressOf()));
}

PunctualShadowMap::~PunctualShadowMap()
{
}

void PunctualShadowMap::Draw()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	auto camera = scene->GetMainCamera() ? scene->GetMainCamera()->GetCamera() : nullptr;
	if (camera == nullptr)
		return;

	// 그림자 캐스터 수집 (스카이/렌더러 없음 제외)
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();
	vector<shared_ptr<GameObject>> casters;
	shared_ptr<Terrain> terrain = nullptr;
	for (auto& go : gameObjects)
	{
		if (go->GetTerrain() != nullptr)
			terrain = go->GetTerrain();
		if (camera->IsCulled(go->GetLayerIndex()))
			continue;
		if (go->GetRenderer() == nullptr || go->GetSkyBox())
			continue;
		casters.push_back(go);
	}

	// 슬롯 할당: 그림자-캐스팅 스팟 라이트 (씬 라이트 순회 — CollectLights 와 동일 순서)
	int32 slot = 0;
	for (auto& lightObj : scene->GetLights())
	{
		auto light = lightObj->GetLight();
		if (light == nullptr)
			continue;
		light->SetShadowSlot(-1); // 기본값 리셋

		if (light->GetLightType() != Spot || light->GetCastShadows() == false)
			continue;
		if (slot >= MAX_PUNCTUAL_SHADOWS)
			continue;

		Matrix V = light->GetSpotView();
		Matrix P = light->GetSpotProj();

		const Matrix T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
		Light::S_SpotVPT[slot] = V * P * T;
		light->SetShadowSlot(slot);

		// 슬롯 깊이 렌더
		_vp.RSSetViewport();
		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		DCT->OMSetRenderTargets(1, nullRTV, _slotDSV[slot].Get());
		DCT->ClearDepthStencilView(_slotDSV[slot].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		RenderContext ctx;
		ctx.tech = 0;
		ctx.view = V;
		ctx.proj = P;
		ctx.light = light;
		ctx.shadowPass = true;

		INSTANCING->Render(ctx, casters);
		if (terrain)
			terrain->TerrainRendererNotPS(V, P);

		slot++;
	}
}
