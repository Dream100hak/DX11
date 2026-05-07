// Sky_VS.hlsl + Sky_PS.hlsl
// 스카이박스 : w=0 으로 카메라 위치 무한 원점 고정

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

struct SkyVSInput
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct SkyOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

// ── VS ───────────────────────────────────────────────────────
SkyOutput VS_Main(SkyVSInput input)
{
    SkyOutput output;
    // w=0 → 이동 무시, 카메라 중심에 고정
    float4 viewPos = mul(float4(input.position.xyz, 0.0f), V);
    float4 clipPos = mul(viewPos, P);
    // z=w 로 설정 → 항상 far plane (depth=1)
    output.position = clipPos.xyww;
    output.uv   = input.uv;
    return output;
}

// ── PS ───────────────────────────────────────────────────────
float4 PS_Main(SkyOutput input) : SV_TARGET
{
    return DiffuseMap.Sample(LinearSampler, input.uv);
}
