// DebugTexture.hlsl
// 텍스처 디버그 출력 ? Albedo 또는 단채널(R) 출력

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

cbuffer DebugBuffer : register(b7)
{
    matrix WVP;   // 풀스크린 쿼드 변환
};

struct DebugVSIn
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
};

struct DebugOut
{
    float4 position : SV_POSITION;
  float2 uv       : TEXCOORD;
};

// ── VS ───────────────────────────────────────────────────────
DebugOut VS_Main(DebugVSIn input)
{
    DebugOut output;
    output.position = mul(float4(input.position.xyz, 1.f), WVP);
    output.uv  = input.uv;
 return output;
}

// ── PS Albedo ────────────────────────────────────────────────
float4 PS_Albedo(DebugOut input) : SV_TARGET
{
    return DiffuseMap.Sample(LinearSampler, input.uv);
}

// ── PS Red 채널 (깊이, SSAO 등 단채널 시각화) ───────────────
float4 PS_Red(DebugOut input) : SV_TARGET
{
 float r = DiffuseMap.Sample(LinearSampler, input.uv).r;
    return float4(r, r, r, 1.f);
}
