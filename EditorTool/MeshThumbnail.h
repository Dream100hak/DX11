#pragma once
#include "Renderer.h"

class MeshThumbnail : public Texture
{
public:
	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

	void Draw(vector<shared_ptr<Renderer>> renderers, Matrix V , Matrix P, shared_ptr<Light> light, vector<shared_ptr<class InstancingBuffer>> buffers);

private:
	void CreateColorTexture();
	void CreateDepthStencilTexture();

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11RenderTargetView> _colorMapRTV;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
	Viewport _vp;
};
