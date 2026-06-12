#include "pch.h"
#include "MeshThumbnail.h"


MeshThumbnail::MeshThumbnail(uint32 width, uint32 height)
	: _width(width), _height(height)
{

	_vp.Set(width, height, GAME->GetSceneDesc().x, GAME->GetSceneDesc().y);

	CreateColorTexture();
	CreateDepthStencilTexture();
}

MeshThumbnail::~MeshThumbnail()
{

}

void MeshThumbnail::Draw(vector<shared_ptr<Renderer>> renderers, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<class InstancingBuffer>> buffers)
{
	if (renderers.size() == 0 || light == nullptr)
		return;

	_vp.RSSetViewport();
	Color color = Color(0.3f, 0.3f, 0.3f, 0.7f);
	DCT->OMSetRenderTargets(1, _colorMapRTV.GetAddressOf(), _depthMapDSV.Get());
	DCT->ClearRenderTargetView(_colorMapRTV.Get(), (float*)&color);
	DCT->ClearDepthStencilView(_depthMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	for (int32 i = 0; i < (int32)renderers.size(); ++i)
	{
		RenderContext ctx;
		ctx.tech   = renderers[i]->GetTechnique();
		ctx.view   = V;
		ctx.proj   = P;
		ctx.light  = light;
		ctx.buffer = buffers[i];

		// 머티리얼 프리뷰 구체(MeshRenderer)는 PS_PreviewLit 강제
		// — 씬용 Standard_PS 는 섀도우맵(미바인딩=0)/라이트배열 의존이라 썸네일이 검게 나옴
		if (renderers[i]->GetRenderType() == RendererType::Mesh)
			ctx.hlslOverride = RESOURCES->Get<HlslShader>(L"MeshPreview_HLSL");

		renderers[i]->Draw(ctx);
	}

	// 즉시 렌더(ImGui 업데이트 중 호출)이므로 메인 RT 복원 필수
	GRAPHICS->RestoreMainRenderTarget();
}

void MeshThumbnail::CreateColorTexture()
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = _width;
	texDesc.Height = _height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> colorMap;

	HRESULT hr = DEVICE->CreateTexture2D(&texDesc, nullptr, colorMap.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateRenderTargetView(colorMap.Get(), nullptr, &_colorMapRTV);
	CHECK(hr);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = DEVICE->CreateShaderResourceView(colorMap.Get(), nullptr, _shaderResourveView.GetAddressOf());
	CHECK(hr);
}

void MeshThumbnail::CreateDepthStencilTexture()
{
	// 같은 크기 썸네일은 깊이버퍼 1장 공유 — 드로우가 순차 실행이고 매 Draw 마다 클리어하므로 안전
	// (개별 보유 시 깊이만 개당 1MB(512²) x 캐시 64 낭비)
	static map<uint64, ComPtr<ID3D11DepthStencilView>> sharedDepth;

	const uint64 key = (static_cast<uint64>(_width) << 32) | _height;
	auto found = sharedDepth.find(key);
	if (found != sharedDepth.end())
	{
		_depthMapDSV = found->second;
		return;
	}

	D3D11_TEXTURE2D_DESC desc = { 0 };
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = _width;
	desc.Height = _height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthMap;

	HRESULT hr = DEVICE->CreateTexture2D(&desc, nullptr, depthMap.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateDepthStencilView(depthMap.Get(), nullptr, &_depthMapDSV);
	CHECK(hr);

	sharedDepth[key] = _depthMapDSV;
}
