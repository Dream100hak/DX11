// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

cbuffer WaterCB : register(b1) { float gLevel; float gSize; float gGrid; float gTime; float4 _wpad; };
float WaveY(float2 p, out float3 nrm)
{
    // 사인파 3개 합성 (방향/주파수/속도 다름)
    float2 d1 = normalize(float2(1, 0.3)), d2 = normalize(float2(-0.6, 1)), d3 = normalize(float2(0.4, -0.8));
    float a1 = 0.18, a2 = 0.10, a3 = 0.06; float f1 = 0.5, f2 = 0.9, f3 = 1.7; float s1 = 1.1, s2 = 1.7, s3 = 2.3;
    float p1 = dot(p, d1) * f1 + gTime * s1, p2 = dot(p, d2) * f2 + gTime * s2, p3 = dot(p, d3) * f3 + gTime * s3;
    float y = a1 * sin(p1) + a2 * sin(p2) + a3 * sin(p3);
    float dx = a1 * f1 * cos(p1) * d1.x + a2 * f2 * cos(p2) * d2.x + a3 * f3 * cos(p3) * d3.x;
    float dz = a1 * f1 * cos(p1) * d1.y + a2 * f2 * cos(p2) * d2.y + a3 * f3 * cos(p3) * d3.y;
    nrm = normalize(float3(-dx, 1.0, -dz));
    return y;
}
struct VOut { float4 pos:SV_POSITION; float3 wp:TEXCOORD0; float3 nrm:TEXCOORD1; };
VOut VSMain(uint vid : SV_VertexID)
{
    uint q = vid / 6u, corner = vid % 6u;
    uint G = (uint)gGrid; uint gx = q % G, gz = q / G;
    float2 co[6] = { float2(0,0), float2(1,0), float2(1,1), float2(0,0), float2(1,1), float2(0,1) };
    float2 c = co[corner];
    float cell = gSize * 2.0 / gGrid;
    float2 xz = float2(-gSize + (gx + c.x) * cell, -gSize + (gz + c.y) * cell);
    float3 n; float y = WaveY(xz, n);
    VOut o; o.wp = float3(xz.x, gLevel + y, xz.y); o.nrm = n; o.pos = mul(float4(o.wp, 1.0), gMVP); return o;
}
float4 PSMain(VOut i) : SV_TARGET
{
    float3 V = normalize(gCamPos.xyz - i.wp);
    float3 N = normalize(i.nrm);
    float fres = pow(saturate(1.0 - dot(N, V)), 5.0); fres = 0.02 + 0.98 * fres; // 슐릭 프레넬
    float3 R = reflect(-V, N);
    float3 skyCol = lerp(gSkyHorizon.rgb, gSkyZenith.rgb, saturate(R.y * 0.5 + 0.5)); // 하늘 반사
    float3 deep = float3(0.02, 0.09, 0.13), shallow = float3(0.05, 0.22, 0.28);
    float3 water = lerp(deep, shallow, saturate(N.y));
    float3 L = normalize(-gLightDir.xyz);
    float spec = pow(saturate(dot(R, L)), 120.0) * gLightDir.w;
    float3 col = lerp(water, skyCol, fres) + spec * gSunColor.rgb;
    // 파도 마루 거품(화이트캡) — 높이가 높고 경사가 큰 곳
    float crest = saturate((i.wp.y - gLevel - 0.12) * 4.0) * saturate((1.0 - N.y) * 6.0);
    col = lerp(col, float3(0.9, 0.95, 1.0), crest * 0.7);
    return float4(col, lerp(0.82, 1.0, crest)); // 거품은 더 불투명
}
