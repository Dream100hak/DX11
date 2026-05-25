#pragma once

class GBuffer
{
public:
	enum { RT_ALBEDO = 0, RT_NORMAL, RT_POSITION, RT_COUNT };

	void Init(uint32 width, uint32 height);

	void BindAsTarget();
	void Clear();

	void BindSRVsPS(uint32 startSlot) const;
	void UnbindSRVsPS(uint32 startSlot) const;

	ComPtr<ID3D11DepthStencilView> GetDSV() const { return _dsv; }
	ComPtr<ID3D11ShaderResourceView> GetSRV(int index) const { return _srvs[index]; }

	uint32 GetWidth() const { return _width; }
	uint32 GetHeight() const { return _height; }

private:
	uint32 _width = 0;
	uint32 _height = 0;

	ComPtr<ID3D11Texture2D>          _textures[RT_COUNT];
	ComPtr<ID3D11RenderTargetView>   _rtvs[RT_COUNT];
	ComPtr<ID3D11ShaderResourceView> _srvs[RT_COUNT];

	ComPtr<ID3D11Texture2D>          _depthTexture;
	ComPtr<ID3D11DepthStencilView>   _dsv;
};
