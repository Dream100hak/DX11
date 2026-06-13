#pragma once
// Cascaded Shadow Map — 디렉셔널 라이트용. 2048² Texture2DArray(CASCADE_COUNT 슬라이스).
// 슬라이스별 DSV 로 캐스케이드마다 깊이 렌더, 배열 SRV 로 디퍼드 라이팅에서 PCF 샘플.
class ShadowMap : public Texture
{
public:
	ShadowMap(uint32 width , uint32 height);
	~ShadowMap();

	// 캐스케이드 슬라이스 DSV (i 번째 캐스케이드 깊이 렌더 대상)
	ComPtr<ID3D11DepthStencilView> GetCascadeDSV(int32 i) { return _cascadeDSV[i]; }
	void Draw();

private:

	uint32 _width;
	uint32 _height;

	ComPtr<ID3D11DepthStencilView> _cascadeDSV[CASCADE_COUNT];

	Viewport _vp;
};
