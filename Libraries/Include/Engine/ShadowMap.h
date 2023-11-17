#pragma once
class ShadowMap : public Texture
{
public:
	ShadowMap(uint32 width , uint32 height);
	~ShadowMap();

	ComPtr<ID3D11DepthStencilView> GetDepthMapDSV() { return _depthMapDSV; }
	void BindDsvAndSetNullRenderTarget();
	void Draw();

private:


private:

	unordered_set<shared_ptr<GameObject>> _shadowObjs; 
	uint32 _width;
	uint32 _height;

	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
};

