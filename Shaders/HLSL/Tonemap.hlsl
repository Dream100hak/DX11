// Tonemap.hlsl
// HDR sceneColor (R16G16B16A16_FLOAT) -> 백버퍼 LDR 톤매핑 패스
// ACES filmic (Narkowicz 근사) + 감마 2.2 인코딩
//
// t0 : SceneColor (HDR, linear)

#include "Common.hlsli"

Texture2D SceneColor : register(t0);

struct TonemapVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

// Fullscreen triangle from SV_VertexID
TonemapVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    TonemapVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// ACES filmic tone mapping (Krzysztof Narkowicz 근사)
float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PS_Main(TonemapVSOutput input) : SV_TARGET
{
    float3 hdr = SceneColor.Sample(PointSampler, input.uv).rgb;
    float3 ldr = ACESFilm(hdr);
    ldr = pow(abs(ldr), 1.0f / 2.2f); // linear -> 감마 인코딩
    return float4(ldr, 1.0f);
}
