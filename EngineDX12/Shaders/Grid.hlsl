// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

struct VOut { float4 pos:SV_POSITION; float2 clip:TEXCOORD0; };
VOut VSMain(uint id : SV_VertexID)
{
    VOut o; float2 p = float2((id << 1) & 2, id & 2);
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0); o.clip = o.pos.xy; return o;
}
struct POut { float4 col : SV_TARGET; float depth : SV_DEPTH; };
float GridA(float2 c, float scale)
{
    float2 coord = c / scale;
    float2 g = abs(frac(coord - 0.5) - 0.5) / fwidth(coord);
    return 1.0 - min(min(g.x, g.y), 1.0);
}
POut PSMain(VOut i)
{
    POut o;
    float4 wn = mul(float4(i.clip, 0.0, 1.0), gInvVP); wn /= wn.w;
    float4 wf = mul(float4(i.clip, 1.0, 1.0), gInvVP); wf /= wf.w;
    float3 ro = gCamPos.xyz, rd = normalize(wf.xyz - wn.xyz);
    if (abs(rd.y) < 1e-5) discard;
    float t = -ro.y / rd.y;
    if (t <= 0.0) discard;
    float3 wp = ro + rd * t;
    float4 cp = mul(float4(wp, 1.0), gMVP); o.depth = cp.z / cp.w;

    float cell = max(gGridParams.x, 0.05);
    float a = max(GridA(wp.xz, cell) * 0.30, GridA(wp.xz, cell * 10.0) * 0.65);
    float3 col = float3(0.55, 0.55, 0.62);
    float ax = 1.0 - min(abs(wp.z) / fwidth(wp.z), 1.0); // z≈0 → X축(빨강)
    float az = 1.0 - min(abs(wp.x) / fwidth(wp.x), 1.0); // x≈0 → Z축(파랑)
    if (ax > 0.1) { col = float3(0.9, 0.25, 0.25); a = max(a, ax); }
    if (az > 0.1) { col = float3(0.25, 0.45, 0.95); a = max(a, az); }
    a *= saturate(1.0 - length(wp.xz - ro.xz) / max(gGridParams.y, 1.0));
    if (a <= 0.001) discard;
    o.col = float4(col, a);
    return o;
}
