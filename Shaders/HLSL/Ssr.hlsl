// Ssr.hlsl
// 스크린스페이스 반사 — 디퍼드 라이팅 직후(sceneColor + GBuffer) 풀스크린.
// 월드공간 레이마치 + 뷰공간 깊이 비교로 히트 → sceneColor 샘플을 반사로 가산.
//   t0: SceneColor(HDR)  t1: Albedo(.a=metallic)  t2: Normal(.a=roughness)  t3: WorldPos(.w=mask)
//   b0: GlobalBuffer (V, VP, VInv)

#include "Common.hlsli"

Texture2D SceneColor      : register(t0);
Texture2D GBufferAlbedo   : register(t1);
Texture2D GBufferNormal   : register(t2);
Texture2D GBufferPosition : register(t3);

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

float4 PS_Main(VSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 scene = SceneColor.Sample(LinearSampler, uv).rgb;

    float4 posS = GBufferPosition.Sample(PointSampler, uv);
    if (posS.w < 0.5f)
        return float4(scene, 1.0f); // 지오메트리 없음(스카이) → 그대로

    float4 nrmS = GBufferNormal.Sample(PointSampler, uv);
    float  rough = nrmS.a;
    if (rough > 0.8f)
        return float4(scene, 1.0f); // 매우 거친 표면은 SSR 생략

    float3 worldPos = posS.xyz;
    float3 N = normalize(nrmS.xyz * 2.0f - 1.0f);
    float  metallic = GBufferAlbedo.Sample(PointSampler, uv).a;

    float3 camPos = CameraPositionWS();
    float3 Vdir = normalize(camPos - worldPos);
    float  NdotV = saturate(dot(N, Vdir));
    float3 R = reflect(-Vdir, N);

    // ── 월드공간 레이마치 (뷰공간 z 로 교차 판정) ──
    const float stepLen = 0.4f;
    const float thickness = 1.5f;
    const int   steps = 48;

    float3 p = worldPos + N * 0.05f; // 셀프-히트 방지 바이어스
    [loop]
    for (int i = 0; i < steps; ++i)
    {
        p += R * stepLen;

        float4 clip = mul(float4(p, 1.0f), VP);
        if (clip.w <= 0.0f)
            break;
        float2 suv = clip.xy / clip.w * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
            break;

        float4 stored = GBufferPosition.Sample(PointSampler, suv);
        if (stored.w < 0.5f)
            continue;

        float marchViewZ  = mul(float4(p, 1.0f), V).z;
        float storedViewZ = mul(float4(stored.xyz, 1.0f), V).z;
        float diff = marchViewZ - storedViewZ; // >0 = 레이가 표면 뒤로 들어감(교차)

        if (diff > 0.0f && diff < thickness)
        {
            float3 refl = SceneColor.Sample(LinearSampler, suv).rgb;

            // 가중: 메탈릭/프레넬(그레이징↑) × (1-거칠기) × 화면 가장자리 페이드
            float  fres = pow(1.0f - NdotV, 4.0f);
            float  reflectivity = lerp(0.04f, 1.0f, metallic);
            float  w = saturate(reflectivity + fres) * (1.0f - rough);

            float2 edgeDist = min(suv, 1.0f - suv);
            w *= saturate(min(edgeDist.x, edgeDist.y) / 0.1f);

            return float4(scene + refl * w, 1.0f);
        }
    }

    return float4(scene, 1.0f); // 미스
}
