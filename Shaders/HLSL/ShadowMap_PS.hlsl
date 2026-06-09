// ShadowMap_PS.hlsl
// ���� Ŭ���� �ִ� ������Ʈ(�Ĺ�, �潺 ��)�� �׸��ڿ� PS
// ������ ������Ʈ�� PS ���� VS �ܵ����� depth write ����

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

// Depth-only 그림자 패스 PS.
// FX(00. ShadowMap.fx)는 PS=NULL 이라 알파클립을 하지 않았다 → 텍스처 없는 오브젝트도 그림자를 캐스팅.
// 동일 동작 유지: 알파클립 머티리얼(UseTexture && UseAlphaClip)일 때만 투명 픽셀을 버린다.
void PS_AlphaClip(float4 pos : SV_POSITION, float2 uv : TEXCOORD0)
{
    if (UseTexture && UseAlphaClip)
    {
        float4 diffuse = DiffuseMap.Sample(LinearSampler, uv);
        clip(diffuse.a - 0.15f);
    }
}
