// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

TextureCube gEnv : register(t2);   // 스카이박스 큐브맵(gExtra.w>0.5 시)
SamplerState gSky : register(s0);
struct VOut { float4 pos:SV_POSITION; float2 clip:TEXCOORD0; };
VOut VSMain(uint id : SV_VertexID)
{
    VOut o;
    float2 p = float2((id << 1) & 2, id & 2); // (0,0)(2,0)(0,2)
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.clip = o.pos.xy;
    return o;
}
float h21(float2 p){ return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float vnoise(float2 p){ float2 i=floor(p), f=frac(p); f=f*f*(3.0-2.0*f);
    float a=h21(i), b=h21(i+float2(1,0)), c=h21(i+float2(0,1)), d=h21(i+float2(1,1));
    return lerp(lerp(a,b,f.x), lerp(c,d,f.x), f.y); }
float fbm(float2 p){ float v=0, a=0.5; [unroll] for(int k=0;k<4;k++){ v+=a*vnoise(p); p*=2.0; a*=0.5; } return v; }
float4 PSMain(VOut i) : SV_TARGET
{
    float4 wn = mul(float4(i.clip, 0.0, 1.0), gInvVP); wn /= wn.w;
    float4 wf = mul(float4(i.clip, 1.0, 1.0), gInvVP); wf /= wf.w;
    float3 dir = normalize(wf.xyz - wn.xyz);

    if (gExtra.w > 0.5) // 큐브맵 스카이박스 — 방향으로 환경맵 샘플
    {
        float3 env = gEnv.SampleLevel(gSky, dir, 0).rgb;
        return float4(env, 1.0);
    }

    float3 horizon = gSkyHorizon.rgb;
    float3 zenith  = gSkyZenith.rgb;
    float3 ground  = horizon * 0.25;
    float3 sky = (dir.y >= 0.0) ? lerp(horizon, zenith, pow(saturate(dir.y), 0.55))
                                : lerp(horizon, ground, saturate(-dir.y * 3.0));
    float3 L = normalize(-gLightDir.xyz);
    float s = saturate(dot(dir, L));
    float sunSize = max(gSkyHorizon.w, 1.0);
    sky += pow(s, sunSize) * 4.0 * gSunColor.rgb;        // 태양 디스크 (색/크기)
    sky += pow(s, 8.0) * 0.25 * gSunColor.rgb;           // 글로우
    // W8 밤 별 (어두울 때 + 천정 방향)
    if (gExtra.z > 0.5 && dir.y > 0.08)
    {
        float st = h21(floor(dir.xz / max(dir.y, 0.2) * 160.0));
        sky += step(0.992, st) * saturate(0.9 - gLightDir.w) * saturate(dir.y) * 2.0;
    }
    // W3 구름 (절차 fbm, 상공)
    if (gDecalCol.w > 0.01 && dir.y > 0.04)
    {
        float2 uv = dir.xz / (dir.y + 0.25) * 0.5 + gGI.y * 0.00012;
        float c = smoothstep(0.45, 0.85, fbm(uv)) * gDecalCol.w * saturate(dir.y * 3.0);
        sky = lerp(sky, float3(1.0, 1.0, 1.05) * (0.6 + 0.4 * s), c * 0.7);
    }
    return float4(sky, 1.0);
}
