// Decal.hlsl
// 디퍼드 데칼 — GBuffer fill 직후, 라이팅 전. 데칼 박스(단위 큐브)를 그려서
// 박스 안의 GBuffer 표면(월드포지션)에 텍스처를 투영해 albedo RT 에 알파 블렌드.
//   t0: GBuffer WorldPos(.w=mask)   t1: Decal 텍스처
//   b0: Global(VP)   b10: DecalBuffer

#include "Common.hlsli"

Texture2D WorldPosTex : register(t0);
Texture2D DecalTex     : register(t1);

cbuffer DecalBuffer : register(b10)
{
    matrix DecalWorld;     // 큐브 로컬 → 월드 (박스 변환)
    matrix DecalInvWorld;  // 월드 → 박스 로컬
    float  Opacity;
    float3 DecalPad;
    float2 InvScreen;
    float2 DecalPad2;
};

struct VSOut
{
    float4 position : SV_POSITION;
};

VSOut VS_Main(float4 pos : POSITION)
{
    VSOut o;
    float4 wp = mul(float4(pos.xyz, 1.0f), DecalWorld);
    o.position = mul(wp, VP);
    return o;
}

float4 PS_Main(VSOut input) : SV_TARGET
{
    int2 px = (int2)input.position.xy;
    float4 wp = WorldPosTex.Load(int3(px, 0));
    if (wp.w < 0.5f)
        discard; // 지오메트리 없음(스카이)

    // 월드 → 박스 로컬 ([-0.5, 0.5]^3 안만 유효)
    float3 local = mul(float4(wp.xyz, 1.0f), DecalInvWorld).xyz;
    if (abs(local.x) > 0.5f || abs(local.y) > 0.5f || abs(local.z) > 0.5f)
        discard;

    // 박스 Y축 투영 → XZ 평면 UV (바닥 데칼)
    float2 duv = local.xz + 0.5f;
    float4 dc = DecalTex.Sample(LinearSampler, duv);

    return float4(dc.rgb, dc.a * Opacity); // albedo 에 알파 블렌드
}
