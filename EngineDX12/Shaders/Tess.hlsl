// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

StructuredBuffer<float> gHeights : register(t1);            // (N+1)^2 하이트맵
cbuffer TessCB : register(b1) { float gN; float gCell; float gHalf; float gTess; float4 _tpad; };

struct CP { float3 pos : POSITION; float2 uv : TEXCOORD0; };
CP VSMain(CP i) { return i; } // 컨트롤포인트 패스스루

struct PatchConst { float edges[4] : SV_TessFactor; float inside[2] : SV_InsideTessFactor; };
// 카메라 거리 기반 적응형 테셀(LOD) — 가까운 패치는 조밀, 먼 패치는 성기게. 엣지별 중점 거리로 산출(인접 패치 크랙 완화).
float EdgeTess(float3 a, float3 b)
{
    float3 mid = (a + b) * 0.5;
    float d = distance(gCamPos.xyz, mid);
    float f = gTess * (16.0 / max(d, 1.0)); // 16m 기준
    return clamp(f, 1.0, 64.0);
}
PatchConst HSConst(InputPatch<CP, 4> ip)
{
    PatchConst p;
    // ip 순서 BL(0) BR(1) TR(2) TL(3) — 엣지: 0=좌(BL-TL),1=하(BL-BR),2=우(BR-TR),3=상(TL-TR)
    p.edges[0] = EdgeTess(ip[0].pos, ip[3].pos);
    p.edges[1] = EdgeTess(ip[0].pos, ip[1].pos);
    p.edges[2] = EdgeTess(ip[1].pos, ip[2].pos);
    p.edges[3] = EdgeTess(ip[3].pos, ip[2].pos);
    p.inside[0] = max(p.edges[0], p.edges[2]);
    p.inside[1] = max(p.edges[1], p.edges[3]);
    return p;
}
[domain("quad")][partitioning("integer")][outputtopology("triangle_cw")][outputcontrolpoints(4)][patchconstantfunc("HSConst")]
CP HSMain(InputPatch<CP, 4> ip, uint i : SV_OutputControlPointID) { return ip[i]; }

float SampleH(float2 uv)
{
    float N = gN; float fx = saturate(uv.x) * N, fz = saturate(uv.y) * N;
    int x0 = (int)floor(fx), z0 = (int)floor(fz);
    int x1 = min(x0 + 1, (int)N), z1 = min(z0 + 1, (int)N);
    float tx = fx - x0, tz = fz - z0; int row = (int)N + 1;
    float h00 = gHeights[z0 * row + x0], h10 = gHeights[z0 * row + x1];
    float h01 = gHeights[z1 * row + x0], h11 = gHeights[z1 * row + x1];
    return lerp(lerp(h00, h10, tx), lerp(h01, h11, tx), tz);
}

struct DSOut { float4 pos : SV_POSITION; float3 wp : TEXCOORD0; float3 nrm : TEXCOORD1; };
[domain("quad")]
DSOut DSMain(PatchConst pc, float2 d : SV_DomainLocation, OutputPatch<CP, 4> ip)
{
    float3 b = lerp(ip[0].pos, ip[1].pos, d.x);   // 하단 엣지(BL→BR)
    float3 t = lerp(ip[3].pos, ip[2].pos, d.x);   // 상단 엣지(TL→TR)
    float3 wp = lerp(b, t, d.y);
    float2 ub = lerp(ip[0].uv, ip[1].uv, d.x), ut = lerp(ip[3].uv, ip[2].uv, d.x);
    float2 uv = lerp(ub, ut, d.y);
    wp.y = SampleH(uv);
    float e = 1.0 / gN;
    float hl = SampleH(uv - float2(e, 0)), hr = SampleH(uv + float2(e, 0));
    float hd = SampleH(uv - float2(0, e)), hu = SampleH(uv + float2(0, e));
    float3 n = normalize(float3(hl - hr, 2.0 * gCell, hd - hu));
    DSOut o; o.pos = mul(float4(wp, 1.0), gMVP); o.wp = wp; o.nrm = n; return o;
}

float4 PSMain(DSOut i) : SV_TARGET
{
    float3 L = normalize(-gLightDir.xyz);
    float ndl = saturate(dot(i.nrm, L));
    float slope = 1.0 - i.nrm.y;
    float3 grass = float3(0.28, 0.42, 0.18), rock = float3(0.34, 0.30, 0.26), snow = float3(0.92, 0.94, 0.97);
    float3 col = lerp(grass, rock, saturate(slope * 2.2));
    col = lerp(col, snow, saturate((i.wp.y - 6.0) / 6.0));
    float3 lit = col * (ndl * gLightDir.w + 0.28);
    return float4(lit, 1.0);
}
