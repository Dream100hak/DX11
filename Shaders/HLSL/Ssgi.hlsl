// Ssgi.hlsl
// 스크린스페이스 GI (1바운스 간접 디퓨즈) — 디퍼드 라이팅 직후(sceneColor + GBuffer) 풀스크린.
// 픽셀 반구에서 코사인 가중 레이를 쏘아 월드공간 레이마치 → 화면 안 지오메트리에 맞으면
// 그 지점의 라이팅된 sceneColor(=2차광원의 방사휘도)를 모아 indirect = albedo × mean(hit) 로 합성.
// 라이팅된 결과를 직접 읽으므로 동적 조명/시간대(라이트 색·방향)에 그대로 반응한다.
//   t0: SceneColor(HDR)  t1: Albedo(.a=metallic)  t2: Normal(.a=roughness)  t3: WorldPos(.w=mask)
//   b0: GlobalBuffer (V, VP, VInv)   b8: SsgiBuffer

#include "Common.hlsli"

Texture2D SceneColor      : register(t0);
Texture2D GBufferAlbedo   : register(t1);
Texture2D GBufferNormal   : register(t2);
Texture2D GBufferPosition : register(t3);

cbuffer SsgiBuffer : register(b8)
{
    float Intensity;
    float Radius;
    float ScreenW;
    float ScreenH;
    float Frame;
    float3 SsgiPad;
};

static const float PI = 3.14159265359f;

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

// 픽셀+프레임 해시 (샘플 회전용 노이즈)
float Hash(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

#define SSGI_RAYS  10
#define SSGI_STEPS 14

float4 PS_Main(VSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 scene = SceneColor.Sample(LinearSampler, uv).rgb;

    float4 posS = GBufferPosition.Sample(PointSampler, uv);
    if (posS.w < 0.5f)
        return float4(scene, 1.0f); // 스카이 → 그대로

    float3 worldPos = posS.xyz;
    float3 N = normalize(GBufferNormal.Sample(PointSampler, uv).xyz * 2.0f - 1.0f);
    float3 albedo = GBufferAlbedo.Sample(PointSampler, uv).rgb;

    // 반구 기저(TBN)
    float3 up = abs(N.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float jitter = Hash(uv * float2(ScreenW, ScreenH) + Frame * 7.13f);

    float  stepLen   = Radius / SSGI_STEPS;
    const float thickness = 1.5f;

    float3 gather = float3(0, 0, 0);

    [loop]
    for (int r = 0; r < SSGI_RAYS; ++r)
    {
        // 코사인 가중 반구 샘플 (회전된 균일 분포)
        float u1 = (r + 0.5f) / SSGI_RAYS;
        float u2 = frac(jitter + r * 0.61803399f); // 황금각 분산
        float rr = sqrt(u1);
        float phi = 2.0f * PI * u2;
        float3 local = float3(rr * cos(phi), rr * sin(phi), sqrt(saturate(1.0f - u1)));
        float3 dir = normalize(local.x * T + local.y * B + local.z * N);

        float3 p = worldPos + N * 0.1f; // 셀프-히트 방지

        [loop]
        for (int s = 0; s < SSGI_STEPS; ++s)
        {
            p += dir * stepLen;

            float4 clip = mul(float4(p, 1.0f), VP);
            if (clip.w <= 0.0f) break;
            float2 suv = clip.xy / clip.w * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
            if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f) break;

            float4 stored = GBufferPosition.Sample(PointSampler, suv);
            if (stored.w < 0.5f) continue; // 스카이 — 통과

            float marchViewZ  = mul(float4(p, 1.0f), V).z;
            float storedViewZ = mul(float4(stored.xyz, 1.0f), V).z;
            float diff = marchViewZ - storedViewZ; // >0 = 표면 뒤로 진입(교차)

            if (diff > 0.02f && diff < thickness)
            {
                // 히트 표면이 이쪽을 향할 때만 (뒷면 누광 방지)
                float3 hitN = normalize(GBufferNormal.Sample(PointSampler, suv).xyz * 2.0f - 1.0f);
                if (dot(hitN, -dir) > 0.0f)
                    gather += SceneColor.Sample(LinearSampler, suv).rgb;
                break;
            }
        }
    }

    // 코사인 가중 샘플링이라 평균이 곧 irradiance/PI → diffuse = albedo × mean(radiance)
    float3 indirect = albedo * (gather / SSGI_RAYS) * Intensity;

    return float4(scene + indirect, 1.0f);
}
