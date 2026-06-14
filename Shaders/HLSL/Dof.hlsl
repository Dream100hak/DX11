// Dof.hlsl
// Depth of Field — Pass 3 직후 HDR sceneColor 에 깊이 기반 가변 블러.
// 깊이(GBuffer worldpos → 뷰공간 z)로 CoC 산출 → 디스크 게더 블러 반경 스케일.
//   t0: SceneColor(HDR)  t1: WorldPos(.w=mask)   b0: Global   b10: DofBuffer

#include "Common.hlsli"

Texture2D SceneColor      : register(t0);
Texture2D GBufferPosition : register(t1);

cbuffer DofBuffer : register(b10)
{
    float FocusDist;   // 초점 거리(뷰공간)
    float FocusRange;  // 이 범위 밖이면 최대 블러
    float MaxCoC;      // 최대 블러 반경(px)
    float DofPad;
    float2 InvScreen;  // (1/w, 1/h)
    float2 DofPad2;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

VSOut VS_Main(uint vertexID : SV_VertexID)
{
    VSOut o;
    o.uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.position = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

// 디스크 게더 샘플(16탭, 황금각 나선)
static const float2 kDisk[16] =
{
    float2( 0.000,  0.000), float2( 0.539,  0.197), float2(-0.219,  0.685), float2(-0.642, -0.314),
    float2( 0.346, -0.731), float2( 0.794,  0.461), float2(-0.668,  0.594), float2(-0.385, -0.847),
    float2( 0.905, -0.205), float2( 0.046,  0.961), float2(-0.918,  0.252), float2( 0.503, -0.832),
    float2( 0.640,  0.730), float2(-0.870, -0.404), float2(-0.117,  0.985), float2( 0.770, -0.560)
};

float ViewZ(float2 uv)
{
    float4 p = GBufferPosition.Sample(PointSampler, uv);
    if (p.w < 0.5f)
        return FocusDist + FocusRange * 4.0f; // 스카이 = 원거리(완전 블러)
    return mul(float4(p.xyz, 1.0f), V).z;
}

float4 PS_Main(VSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 sharp = SceneColor.Sample(LinearSampler, uv).rgb;

    float vz = ViewZ(uv);
    float coc = saturate(abs(vz - FocusDist) / max(FocusRange, 0.001f));
    float radius = coc * MaxCoC;

    if (radius < 0.5f)
        return float4(sharp, 1.0f); // 초점 영역 — 선명

    // 가변 반경 디스크 게더 (배경 누출 완화: 더 먼 샘플만 가중)
    float3 sum = float3(0, 0, 0);
    float wsum = 0.0f;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 off = kDisk[i] * radius * InvScreen;
        float2 suv = uv + off;
        float3 c = SceneColor.Sample(LinearSampler, suv).rgb;
        // 샘플이 초점보다 앞(더 가까움)이면 가중 낮춰 전경 번짐 억제
        float scoc = saturate(abs(ViewZ(suv) - FocusDist) / max(FocusRange, 0.001f));
        float w = max(scoc, coc * 0.3f);
        sum += c * w;
        wsum += w;
    }
    float3 blurred = (wsum > 0.0001f) ? sum / wsum : sharp;

    return float4(lerp(sharp, blurred, coc), 1.0f);
}
