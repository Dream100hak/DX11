// SsaoBlur.hlsl
// SSAO 맵 양방향(가로/세로) edge-preserving 블러 (FX 00. SsaoBlur.fx 대체)
// 노멀/깊이 불연속 경계는 블러에서 제외 (bilateral)
// HorzBlur: 1 = 가로, 0 = 세로 (FX 의 uniform bool 테크닉 분기 대체)

cbuffer SsaoBlurBuffer : register(b8)
{
    float TexelWidth;
    float TexelHeight;
    float HorzBlur;
    float BlurPad;
};

static const float Weights[11] =
{
    0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f
};

Texture2D NormalDepthMap : register(t0);
Texture2D InputImage     : register(t1);

SamplerState samClamp : register(s0); // LINEAR_MIP_POINT, CLAMP

struct VertexSsao
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

VertexOut VS_Main(VertexSsao vin)
{
    VertexOut vout;
    // Already in NDC space.
    vout.PosH = float4(vin.pos, 1.0f);
    vout.uv = vin.uv;
    return vout;
}

float4 PS_Main(VertexOut pin) : SV_Target
{
    float2 texOffset;
    if (HorzBlur > 0.5f)
        texOffset = float2(TexelWidth, 0.0f);
    else
        texOffset = float2(0.0f, TexelHeight);

    const int gBlurRadius = 5;

    // 중앙 샘플은 항상 기여
    float4 color = Weights[5] * InputImage.SampleLevel(samClamp, pin.uv, 0.0);
    float totalWeight = Weights[5];

    float4 centerNormalDepth = NormalDepthMap.SampleLevel(samClamp, pin.uv, 0.0f);

    [unroll]
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0)
            continue;

        float2 tex = pin.uv + i * texOffset;

        float4 neighborNormalDepth = NormalDepthMap.SampleLevel(samClamp, tex, 0.0f);

        // 노멀/깊이가 크게 다르면 불연속 경계 → 블러 제외
        if (dot(neighborNormalDepth.xyz, centerNormalDepth.xyz) >= 0.8f &&
            abs(neighborNormalDepth.a - centerNormalDepth.a) <= 0.2f)
        {
            float weight = Weights[i + gBlurRadius];
            color += weight * InputImage.SampleLevel(samClamp, tex, 0.0);
            totalWeight += weight;
        }
    }

    // 제외된 샘플 보정 (가중치 합 = 1)
    return color / totalWeight;
}
