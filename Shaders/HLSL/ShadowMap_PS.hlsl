// ShadowMap_PS.hlsl
// 알파 클립이 있는 오브젝트(식물, 펜스 등)의 그림자용 PS
// 불투명 오브젝트는 PS 없이 VS 단독으로 depth write 가능

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

// PS 없음 버전 : null PS 바인딩 시 이 파일 불필요
// 알파클립 오브젝트용 PS
void PS_AlphaClip(float4 pos : SV_POSITION, float2 uv : TEXCOORD0)
{
    float4 diffuse = DiffuseMap.Sample(LinearSampler, uv);
    clip(diffuse.a - 0.15f);
}
