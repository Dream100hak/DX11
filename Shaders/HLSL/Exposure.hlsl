// Exposure.hlsl
// Auto-exposure 1단계 — sceneColor 의 로그 휘도를 밉맵 텍스처 mip0 에 기록.
// 이후 GenerateMips 로 평균(최상위 밉)을 구하고, Tonemap 이 노출을 산출한다.

#include "Common.hlsli"

Texture2D SceneColor : register(t0);

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

float PS_Luminance(VSOut input) : SV_TARGET
{
    float3 c = SceneColor.SampleLevel(LinearSampler, input.uv, 0.0f).rgb;
    float lum = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    return log(max(lum, 1e-2f)); // 하한 상향 — 검은 여백/그늘이 기하평균을 폭락시키는 것 방지
}

// ── Auto-exposure 2단계: 시간적 적응(눈 적응) ──
// t0: 로그휘도 밉체인(평균=최상위 밉)  t1: 직전 프레임 적응 휘도(1x1, linear)
//   adapted = lerp(prev, avgLum, 1-exp(-dt*speed))  → Tonemap 이 이 1x1 을 읽어 노출 산출
Texture2D PrevAdapt : register(t1);

cbuffer AdaptBuffer : register(b10)
{
    float DeltaTime;
    float AdaptSpeed;
    float2 AdaptPad;
};

float PS_Adapt(VSOut input) : SV_TARGET
{
    float avgLogLum = SceneColor.SampleLevel(LinearSampler, float2(0.5f, 0.5f), 20.0f).r; // t0=lumTex 최상위 밉
    float avgLum = exp(avgLogLum);
    float prev = PrevAdapt.SampleLevel(PointSampler, float2(0.5f, 0.5f), 0.0f).r;
    if (prev <= 1e-4f)
        return avgLum; // 초기 프레임 스냅
    float rate = saturate(1.0f - exp(-DeltaTime * AdaptSpeed));
    return prev + (avgLum - prev) * rate;
}
