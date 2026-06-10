// PostProcess.hlsl
// Bloom 포스트프로세싱 — BrightPass(하프 해상도 추출) + 분리형 가우시안 블러
//
// t0 : 입력 텍스처 (BrightPass: HDR sceneColor / Blur: 이전 단계 결과)
// b8 : PostProcessBuffer (C++ PostProcessDesc 와 일치)

#include "Common.hlsli"

Texture2D InputTex : register(t0);

cbuffer PostProcessBuffer : register(b8)
{
    float2 TexelSize;      // 1/width, 1/height (입력 텍스처 기준)
    float  BloomThreshold; // 휘도 임계값 (HDR, 보통 1.0)
    float  BloomIntensity; // 합성 강도 (BrightPass 출력에 곱)
};

struct PostVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

PostVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    PostVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// ── BrightPass: 임계값 이상 휘도만 추출 (soft knee) ─────────────────────────
float4 PS_BrightPass(PostVSOutput input) : SV_TARGET
{
    float3 color = InputTex.Sample(LinearSampler, input.uv).rgb;
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float contribution = saturate((luma - BloomThreshold) / 0.5f); // soft knee
    return float4(color * contribution * BloomIntensity, 1.0f);
}

// ── 분리형 가우시안 블러 (9-tap) ────────────────────────────────────────────
static const float BlurWeights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

float3 GaussianBlur(float2 uv, float2 dir)
{
    float3 result = InputTex.Sample(LinearSampler, uv).rgb * BlurWeights[0];
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = dir * TexelSize * i;
        result += InputTex.Sample(LinearSampler, uv + offset).rgb * BlurWeights[i];
        result += InputTex.Sample(LinearSampler, uv - offset).rgb * BlurWeights[i];
    }
    return result;
}

float4 PS_BlurH(PostVSOutput input) : SV_TARGET
{
    return float4(GaussianBlur(input.uv, float2(1, 0)), 1.0f);
}

float4 PS_BlurV(PostVSOutput input) : SV_TARGET
{
    return float4(GaussianBlur(input.uv, float2(0, 1)), 1.0f);
}
