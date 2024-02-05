#pragma once

class ModelRenderer;

class MeshThumbnail : public Texture
{
public:
	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

	void SetModelAndCam(shared_ptr<ModelRenderer> renderer, shared_ptr<Camera> cam);
	void SetWorldMatrix(const Matrix& world) { _world = world; }

	void Draw();

private:
	void CreateColorTexture();
	void CreateDepthStencilTexture();

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11RenderTargetView> _colorMapRTV;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;

	shared_ptr<ModelRenderer> _modelRenderer; 
	shared_ptr<Camera> _cam;

	Viewport _vp;

	Matrix _world;
};

