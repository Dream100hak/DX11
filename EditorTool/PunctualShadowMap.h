#pragma once
// 점/스팟 라이트 그림자 — 현재는 스팟 라이트 원근 섀도우맵 (Texture2DArray, MAX_PUNCTUAL_SHADOWS 슬라이스).
// Draw 가 씬의 그림자-캐스팅 스팟 라이트에 슬롯을 할당하고 슬롯별 깊이를 렌더.
// (포인트 라이트 큐브맵은 다음 단계)
class PunctualShadowMap : public Texture
{
public:
	PunctualShadowMap(uint32 size);
	~PunctualShadowMap();

	void Draw(); // 스팟 그림자 슬롯 할당 + 슬롯별 깊이 렌더 + Light::S_SpotVPT 채움

private:
	uint32 _size;
	ComPtr<ID3D11DepthStencilView> _slotDSV[MAX_PUNCTUAL_SHADOWS];
	Viewport _vp;
};
