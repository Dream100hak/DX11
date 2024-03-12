#pragma once

class ModelRenderer;
class MeshRenderer;

#include "MeshRenderer.h"

class MeshThumbnail : public Texture
{
public:
	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

	template<typename T>
	void Draw(shared_ptr<T> renderer, shared_ptr<Camera> camera, shared_ptr<Light> light, shared_ptr<class InstancingBuffer> buffer);

private:
	void CreateColorTexture();
	void CreateDepthStencilTexture();

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11RenderTargetView> _colorMapRTV;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
	Viewport _vp;
};

template<typename T>
void MeshThumbnail::Draw(shared_ptr<T> renderer, shared_ptr<Camera> camera, shared_ptr<Light> light, shared_ptr<class InstancingBuffer> buffer)
{
	if (renderer == nullptr && camera == nullptr && light == nullptr)
		return;

	_vp.RSSetViewport();

	DCT->OMSetRenderTargets(1, _colorMapRTV.GetAddressOf(), _depthMapDSV.Get());
	DCT->ClearRenderTargetView(_colorMapRTV.Get(), (float*)(&GAME->GetGameDesc().clearColor));
	DCT->ClearDepthStencilView(_depthMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	renderer->ThumbnailRender(camera, light, buffer);

}

