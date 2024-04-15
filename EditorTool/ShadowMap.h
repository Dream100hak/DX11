#pragma once
class ShadowMap : public Texture
{
public:
	ShadowMap(uint32 width , uint32 height);
	~ShadowMap();


	ComPtr<ID3D11DepthStencilView> GetDepthMapDSV() { return _depthMapDSV; }
	void Draw();

private:

	uint32 _width;
	uint32 _height;

	ComPtr<ID3D11DepthStencilView> _depthMapDSV;

	Viewport _vp;
};

