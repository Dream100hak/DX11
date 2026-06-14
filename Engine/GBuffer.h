#pragma once

class GBuffer
{
public:
	enum { RT_ALBEDO = 0, RT_NORMAL, RT_POSITION, RT_EMISSIVE, RT_COUNT };

	// 디퍼드 라이팅 입력으로 연속 바인딩(t0~t2)되는 RT 수 — Emissive 는 t3(Shadow)/t4(Ssao) 와
	// 충돌하지 않게 별도 슬롯(t8)에 바인딩한다 (Camera::Render_Deferred)
	enum { RT_LIGHTING_COUNT = 3 };

	void Init(uint32 width, uint32 height);

	void BindAsTarget();
	void Clear();

	void BindSRVsPS(uint32 startSlot) const;
	void UnbindSRVsPS(uint32 startSlot) const;

	ComPtr<ID3D11DepthStencilView> GetDSV() const { return _dsv; }
	ComPtr<ID3D11ShaderResourceView> GetSRV(int index) const { return _srvs[index]; }
	ComPtr<ID3D11RenderTargetView> GetRTV(int index) const { return _rtvs[index]; } // 데칼: 알베도 RT 단독 바인딩용

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
