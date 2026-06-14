#include "pch.h"
#include "PunctualShadowMap.h"
#include "Camera.h"
#include "Light.h"
#include "Terrain.h"

PunctualShadowMap::PunctualShadowMap(uint32 spotSize, uint32 pointSize)
	: _spotSize(spotSize), _pointSize(pointSize)
{
	_spotVp.Set(static_cast<float>(spotSize), static_cast<float>(spotSize));
	_pointVp.Set(static_cast<float>(pointSize), static_cast<float>(pointSize));

	CreateSpotArray();
	CreatePointCubeArray();
}

PunctualShadowMap::~PunctualShadowMap()
{
}

void PunctualShadowMap::CreateSpotArray()
{
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = _spotSize;
	texDesc.Height = _spotSize;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = MAX_PUNCTUAL_SHADOWS;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> tex;
	CHECK(DEVICE->CreateTexture2D(&texDesc, 0, tex.GetAddressOf()));

	for (int32 i = 0; i < MAX_PUNCTUAL_SHADOWS; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		CHECK(DEVICE->CreateDepthStencilView(tex.Get(), &dsvDesc, _spotDSV[i].GetAddressOf()));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = MAX_PUNCTUAL_SHADOWS;
	CHECK(DEVICE->CreateShaderResourceView(tex.Get(), &srvDesc, _shaderResourveView.GetAddressOf()));
}

void PunctualShadowMap::CreatePointCubeArray()
{
	// 큐브 배열 = ArraySize 6×N, MISC_TEXTURECUBE
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = _pointSize;
	texDesc.Height = _pointSize;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = MAX_PUNCTUAL_SHADOWS * 6;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	ComPtr<ID3D11Texture2D> tex;
	CHECK(DEVICE->CreateTexture2D(&texDesc, 0, tex.GetAddressOf()));

	// 면별 DSV (큐브 c, 면 f → 배열 슬라이스 c*6+f)
	for (int32 i = 0; i < MAX_PUNCTUAL_SHADOWS * 6; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		CHECK(DEVICE->CreateDepthStencilView(tex.Get(), &dsvDesc, _pointFaceDSV[i].GetAddressOf()));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = 0;
	srvDesc.TextureCubeArray.MipLevels = 1;
	srvDesc.TextureCubeArray.First2DArrayFace = 0;
	srvDesc.TextureCubeArray.NumCubes = MAX_PUNCTUAL_SHADOWS;
	CHECK(DEVICE->CreateShaderResourceView(tex.Get(), &srvDesc, _pointCubeSRV.GetAddressOf()));
}

void PunctualShadowMap::Draw()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	auto camera = scene->GetMainCamera() ? scene->GetMainCamera()->GetCamera() : nullptr;
	if (camera == nullptr)
		return;

	// 그림자 캐스터 수집
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

	// 모든 라이트 슬롯 리셋
	for (auto& lightObj : scene->GetLights())
		if (lightObj->GetLight())
			lightObj->GetLight()->SetShadowSlot(-1);

	const Matrix T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	auto renderDepth = [&](const Matrix& V, const Matrix& P, ID3D11DepthStencilView* dsv, Viewport vp)
	{
		vp.RSSetViewport();
		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		DCT->OMSetRenderTargets(1, nullRTV, dsv);
		DCT->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

		RenderContext ctx;
		ctx.tech = 0;
		ctx.view = V;
		ctx.proj = P;
		ctx.shadowPass = true;
		INSTANCING->Render(ctx, casters);
		if (terrain)
			terrain->TerrainRendererNotPS(V, P);
	};

	// ── 스팟 슬롯 ──
	int32 spotSlot = 0;
	for (auto& lightObj : scene->GetLights())
	{
		auto light = lightObj->GetLight();
		if (light == nullptr || light->GetLightType() != Spot || light->GetCastShadows() == false)
			continue;
		if (spotSlot >= MAX_PUNCTUAL_SHADOWS)
			break;

		Matrix V = light->GetSpotView();
		Matrix P = light->GetSpotProj();
		Light::S_SpotVPT[spotSlot] = V * P * T;
		light->SetShadowSlot(spotSlot);
		renderDepth(V, P, _spotDSV[spotSlot].Get(), _spotVp);
		spotSlot++;
	}

	// ── 포인트 큐브 슬롯 (6면) ──
	static const Vec3 faceDir[6] = {
		Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1) };
	static const Vec3 faceUp[6] = {
		Vec3(0,1,0), Vec3(0,1,0), Vec3(0,0,-1), Vec3(0,0,1), Vec3(0,1,0), Vec3(0,1,0) };

	int32 pointSlot = 0;
	for (auto& lightObj : scene->GetLights())
	{
		auto light = lightObj->GetLight();
		if (light == nullptr || light->GetLightType() != Point || light->GetCastShadows() == false)
			continue;
		if (pointSlot >= MAX_PUNCTUAL_SHADOWS)
			break;

		Vec3 pos = lightObj->GetTransform()->GetPosition();
		float nearZ = 0.1f;
		float farZ = max(light->GetRange(), nearZ + 1.f);
		Matrix P = ::XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, nearZ, farZ);

		for (int32 f = 0; f < 6; ++f)
		{
			Matrix V = ::XMMatrixLookAtLH(pos, pos + faceDir[f], faceUp[f]);
			renderDepth(V, P, _pointFaceDSV[pointSlot * 6 + f].Get(), _pointVp);
		}
		light->SetShadowSlot(pointSlot);
		pointSlot++;
	}
}
