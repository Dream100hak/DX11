#include "pch.h"
#include "Ibl.h"
#include "BindShaderDesc.h"

namespace
{
	// (mip 별) 큐브 RT 생성 헬퍼 — face/mip 단위 RTV + 큐브 SRV
	struct CubeRT
	{
		ComPtr<ID3D11Texture2D> tex;
		vector<ComPtr<ID3D11RenderTargetView>> rtvs; // [mip * 6 + face]
		ComPtr<ID3D11ShaderResourceView> srv;
	};

	CubeRT CreateCubeRT(uint32 size, uint32 mips)
	{
		CubeRT rt;

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = size;
		desc.Height = size;
		desc.MipLevels = mips;
		desc.ArraySize = 6;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		CHECK(DEVICE->CreateTexture2D(&desc, nullptr, rt.tex.GetAddressOf()));

		rt.rtvs.resize(mips * 6);
		for (uint32 mip = 0; mip < mips; ++mip)
		{
			for (uint32 face = 0; face < 6; ++face)
			{
				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
				rtvDesc.Format = desc.Format;
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.MipSlice = mip;
				rtvDesc.Texture2DArray.FirstArraySlice = face;
				rtvDesc.Texture2DArray.ArraySize = 1;
				CHECK(DEVICE->CreateRenderTargetView(rt.tex.Get(), &rtvDesc, rt.rtvs[mip * 6 + face].GetAddressOf()));
			}
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = mips;
		CHECK(DEVICE->CreateShaderResourceView(rt.tex.Get(), &srvDesc, rt.srv.GetAddressOf()));

		return rt;
	}

	shared_ptr<ConstantBuffer<IblBakeDesc>> g_bakeCB;

	void BakeFace(shared_ptr<HlslShader>& shader, ID3D11RenderTargetView* rtv,
		ID3D11ShaderResourceView* env, uint32 size, int32 face, float roughness)
	{
		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(size);
		vp.Height = static_cast<float>(size);
		vp.MaxDepth = 1.f;
		DCT->RSSetViewports(1, &vp);
		DCT->OMSetRenderTargets(1, &rtv, nullptr);

		IblBakeDesc bake;
		bake.faceIndex = face;
		bake.roughness = roughness;
		g_bakeCB->CopyData(bake);

		shader->Bind();
		shader->SetPSSRV(0, env);
		shader->SetPSConstantBuffer(8, g_bakeCB->GetComPtr().Get());

		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->Draw(3, 0);
		shader->SetPSSRV(0, nullptr);
	}
}

void Ibl::Init(const wstring& envCubePath)
{
	// 환경 큐브맵 로드 (DDS cube)
	DirectX::ScratchImage image;
	HRESULT hr = DirectX::LoadFromDDSFile(envCubePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	if (FAILED(hr))
		return; // 환경맵 없으면 IBL 비활성 (UseIbl=0 폴백)

	CHECK(DirectX::CreateShaderResourceView(DEVICE.Get(), image.GetImages(), image.GetImageCount(),
		image.GetMetadata(), _envSRV.GetAddressOf()));

	g_bakeCB = make_shared<ConstantBuffer<IblBakeDesc>>();
	g_bakeCB->Create();

	RENDER_STATES->BindAllSamplersPS();

	BakeIrradiance();
	BakePrefiltered();
	BakeBrdfLut();

	g_bakeCB = nullptr;
	GRAPHICS->RestoreMainRenderTarget();

	_ready = true;
}

void Ibl::BakeIrradiance()
{
	auto shader = RESOURCES->Get<HlslShader>(L"IblIrradiance_HLSL");
	if (!shader) return;

	CubeRT rt = CreateCubeRT(IRRADIANCE_SIZE, 1);
	for (int32 face = 0; face < 6; ++face)
		BakeFace(shader, rt.rtvs[face].Get(), _envSRV.Get(), IRRADIANCE_SIZE, face, 0.f);

	_irradianceSRV = rt.srv;
}

void Ibl::BakePrefiltered()
{
	auto shader = RESOURCES->Get<HlslShader>(L"IblPrefilter_HLSL");
	if (!shader) return;

	CubeRT rt = CreateCubeRT(PREFILTER_SIZE, PREFILTER_MIPS);
	for (uint32 mip = 0; mip < PREFILTER_MIPS; ++mip)
	{
		uint32 mipSize = PREFILTER_SIZE >> mip;
		float roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIPS - 1);
		for (int32 face = 0; face < 6; ++face)
			BakeFace(shader, rt.rtvs[mip * 6 + face].Get(), _envSRV.Get(), mipSize, face, roughness);
	}

	_prefilteredSRV = rt.srv;
}

void Ibl::BakeBrdfLut()
{
	auto shader = RESOURCES->Get<HlslShader>(L"IblBrdf_HLSL");
	if (!shader) return;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = BRDF_LUT_SIZE;
	desc.Height = BRDF_LUT_SIZE;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16G16_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> tex;
	ComPtr<ID3D11RenderTargetView> rtv;
	CHECK(DEVICE->CreateTexture2D(&desc, nullptr, tex.GetAddressOf()));
	CHECK(DEVICE->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf()));
	CHECK(DEVICE->CreateShaderResourceView(tex.Get(), nullptr, _brdfSRV.GetAddressOf()));

	BakeFace(shader, rtv.Get(), _envSRV.Get(), BRDF_LUT_SIZE, 0, 0.f);
}
