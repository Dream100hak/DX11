// Fxaa.hlsl
// FXAA 3.11 (간소화 PC quality) — 톤매핑 후 LDR 이미지의 엣지 안티앨리어싱
//
// t0 : LDR 입력 (톤매핑 결과, 감마 공간)
// b8 : PostProcessBuffer (TexelSize 만 사용)

#include "Common.hlsli"

Texture2D InputTex : register(t0);

cbuffer PostProcessBuffer : register(b8)
{
    float2 TexelSize;
    float  BloomThreshold;
    float  BloomIntensity;
};

struct FxaaVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

FxaaVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    FxaaVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float Luma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

static const float EDGE_THRESHOLD_MIN = 0.0312f;
static const float EDGE_THRESHOLD_MAX = 0.125f;
static const float SUBPIXEL_QUALITY   = 0.75f;

float4 PS_Main(FxaaVSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 colorCenter = InputTex.Sample(LinearSampler, uv).rgb;

    float lumaCenter = Luma(colorCenter);
    float lumaDown   = Luma(InputTex.Sample(LinearSampler, uv + float2(0, TexelSize.y)).rgb);
    float lumaUp     = Luma(InputTex.Sample(LinearSampler, uv - float2(0, TexelSize.y)).rgb);
    float lumaLeft   = Luma(InputTex.Sample(LinearSampler, uv - float2(TexelSize.x, 0)).rgb);
    float lumaRight  = Luma(InputTex.Sample(LinearSampler, uv + float2(TexelSize.x, 0)).rgb);

    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    // 엣지가 아니면 그대로
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
        return float4(colorCenter, 1.0f);

    float lumaDownLeft  = Luma(InputTex.Sample(LinearSampler, uv + float2(-TexelSize.x,  TexelSize.y)).rgb);
    float lumaUpRight   = Luma(InputTex.Sample(LinearSampler, uv + float2( TexelSize.x, -TexelSize.y)).rgb);
    float lumaUpLeft    = Luma(InputTex.Sample(LinearSampler, uv + float2(-TexelSize.x, -TexelSize.y)).rgb);
    float lumaDownRight = Luma(InputTex.Sample(LinearSampler, uv + float2( TexelSize.x,  TexelSize.y)).rgb);

    float lumaDownUp    = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;
    float lumaLeftCorners  = lumaDownLeft + lumaUpLeft;
    float lumaDownCorners  = lumaDownLeft + lumaDownRight;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners    = lumaUpRight + lumaUpLeft;

    float edgeHorizontal = abs(-2.0f * lumaLeft + lumaLeftCorners) + abs(-2.0f * lumaCenter + lumaDownUp) * 2.0f + abs(-2.0f * lumaRight + lumaRightCorners);
    float edgeVertical   = abs(-2.0f * lumaUp + lumaUpCorners) + abs(-2.0f * lumaCenter + lumaLeftRight) * 2.0f + abs(-2.0f * lumaDown + lumaDownCorners);
    bool isHorizontal = (edgeHorizontal >= edgeVertical);

    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25f * max(abs(gradient1), abs(gradient2));

    float stepLength = isHorizontal ? TexelSize.y : TexelSize.x;
    float lumaLocalAverage = 0.0f;
    if (is1Steepest)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5f * (luma1 + lumaCenter);
    }
    else
    {
        lumaLocalAverage = 0.5f * (luma2 + lumaCenter);
    }

    float2 currentUv = uv;
    if (isHorizontal)
        currentUv.y += stepLength * 0.5f;
    else
        currentUv.x += stepLength * 0.5f;

    // 엣지 양방향 탐색
    float2 offset = isHorizontal ? float2(TexelSize.x, 0) : float2(0, TexelSize.y);
    float2 uv1 = currentUv - offset;
    float2 uv2 = currentUv + offset;

    float lumaEnd1 = Luma(InputTex.Sample(LinearSampler, uv1).rgb) - lumaLocalAverage;
    float lumaEnd2 = Luma(InputTex.Sample(LinearSampler, uv2).rgb) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        if (!reached1)
        {
            uv1 -= offset;
            lumaEnd1 = Luma(InputTex.Sample(LinearSampler, uv1).rgb) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!reached2)
        {
            uv2 += offset;
            lumaEnd2 = Luma(InputTex.Sample(LinearSampler, uv2).rgb) - lumaLocalAverage;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
        if (reached1 && reached2)
            break;
    }

    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = (distance1 + distance2);
    float pixelOffset = -distanceFinal / edgeThickness + 0.5f;

    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0f;

    // 서브픽셀 AA
    float lumaAverage = (1.0f / 12.0f) * (2.0f * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = saturate(abs(lumaAverage - lumaCenter) / lumaRange);
    float subPixelOffset2 = (-2.0f * subPixelOffset1 + 3.0f) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
    finalOffset = max(finalOffset, subPixelOffsetFinal);

    float2 finalUv = uv;
    if (isHorizontal)
        finalUv.y += finalOffset * stepLength;
    else
        finalUv.x += finalOffset * stepLength;

    float3 finalColor = InputTex.Sample(LinearSampler, finalUv).rgb;
    return float4(finalColor, 1.0f);
}
