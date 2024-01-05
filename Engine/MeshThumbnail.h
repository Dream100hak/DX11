#pragma once
class MeshThumbnail : public Texture
{
public:

	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

private:

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
	Viewport _vp;

};

