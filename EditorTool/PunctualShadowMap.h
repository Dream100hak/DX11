#pragma once
// 점/스팟 라이트 그림자.
//  - 스팟: 원근 섀도우맵 (Texture2DArray, MAX_PUNCTUAL_SHADOWS 슬롯) — 베이스 Texture SRV
//  - 포인트: 큐브 섀도우맵 (TextureCubeArray, MAX_PUNCTUAL_SHADOWS 큐브 × 6면) — _pointCubeSRV
// Draw 가 그림자-캐스팅 점/스팟 라이트에 슬롯을 할당하고 깊이를 렌더.
class PunctualShadowMap : public Texture
{
public:
	PunctualShadowMap(uint32 spotSize, uint32 pointSize);
	~PunctualShadowMap();

	void Draw();

	ComPtr<ID3D11ShaderResourceView> GetPointCubeSRV() { return _pointCubeSRV; }

private:
	void CreateSpotArray();
	void CreatePointCubeArray();

	uint32 _spotSize;
	uint32 _pointSize;

	// 스팟 (원근 2D 배열) — SRV 는 베이스 Texture::_shaderResourveView
	ComPtr<ID3D11DepthStencilView> _spotDSV[MAX_PUNCTUAL_SHADOWS];
	Viewport _spotVp;

	// 포인트 (큐브 배열) — 6면 × 큐브 수 만큼의 DSV + 큐브배열 SRV
	ComPtr<ID3D11DepthStencilView> _pointFaceDSV[MAX_PUNCTUAL_SHADOWS * 6];
	ComPtr<ID3D11ShaderResourceView> _pointCubeSRV;
	Viewport _pointVp;
};
