#include "D3D12Device.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

using namespace DirectX;

// SceneCB 는 D3D12Device.h 로 이동 (Render.cpp 공용)

// 공용 SceneCB 정의 (그래픽스/컴퓨트 동일 레이아웃)
static const char* kSceneCB = R"(
cbuffer SceneCB : register(b0)
{
    row_major float4x4 gMVP;
    row_major float4x4 gModel;
    float4 gLightDir;   // xyz dir, w intensity
    float4 gCamPos;
    float4 gGridMin;
    float4 gGridMax;
    float4 gGridDim;    // x,y,z probe counts, w rays/probe
    float4 gGI;         // x giStrength, y frame, z ambient
    row_major float4x4 gInvVP; // 역 뷰프로젝션 (스카이 레이 복원)
    float4 gPointPos;   // xyz 점광원 위치, w 반경
    float4 gPointColor; // rgb 색, w 세기
    float4 gMatParams;  // x metallic, y roughness, z emissive, w albedoTint
    float4 gSunColor;   // rgb 태양색 (세기는 gLightDir.w)
    float4 gFog;        // rgb 안개색, w 밀도
    float4 gGrade;      // x 대비, y 채도, z 색온도, w 비네트
    float4 gSkyZenith;  // rgb 천정색, w 소프트섀도 반경
    float4 gSkyHorizon; // rgb 지평선색, w 태양 크기(지수)
    float4 gDebug;      // x 디버그뷰, y 프로브뷰, z 톤맵op, w 환경강도
    float4 gSpotPos;    // xyz 스팟 위치, w 반경
    float4 gSpotDir;    // xyz 스팟 방향, w cos(콘각)
    float4 gSpotColor;  // rgb 색×세기, w on
    float4 gTint;       // rgb 디퓨즈 틴트
    float4 gPtPos[4];   // 다중 점광원 위치+반경
    float4 gPtCol[4];   // 다중 점광원 색×세기 (w on)
};
)";

static const std::string kMeshShader = std::string(kSceneCB) + R"(
#define OCT 8
struct ProbeSH { float3 c0; float3 c1; float3 c2; float3 c3; }; // SH-L1: DC + (x,y,z) 방향 계수

RaytracingAccelerationStructure gScene  : register(t0); // TLAS (RayQuery 그림자)
StructuredBuffer<ProbeSH>       gProbes : register(t1); // DDGI 프로브 (SH-L1 irradiance)
Texture2D                       gDiffuse   : register(t2); // 디퓨즈
Texture2D                       gNormalMap : register(t3); // 노멀맵
Texture2D                       gSpecMap   : register(t4); // 스펙큘러
StructuredBuffer<float2>        gProbeDepth : register(t5); // 옥타헤드럴 depth (mean, mean²)
SamplerState                    gSamp    : register(s0);
cbuffer UseTexCB : register(b1) { uint gUseTex; };       // 루트 상수 (1=텍스처)

// 단위벡터 → 옥타헤드럴 [0,1]² (depth 텍셀 인덱싱)
float2 OctEncode(float3 d)
{
    d /= (abs(d.x) + abs(d.y) + abs(d.z));
    float2 o = (d.z >= 0.0) ? d.xy : (1.0 - abs(d.yx)) * sign(d.xy);
    return o * 0.5 + 0.5;
}

struct VSIn  { float3 pos : POSITION; float3 nrm : NORMAL; float3 col : COLOR; float2 uv : TEXCOORD; float3 tan : TANGENT; };
struct VSOut { float4 pos : SV_POSITION; float3 wnrm : NORMAL; float3 wpos : TEXCOORD0; float3 col : COLOR; float2 uv : TEXCOORD1; float3 wtan : TANGENT; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos  = mul(float4(i.pos, 1.0), gMVP);
    o.wnrm = mul(i.nrm, (float3x3)gModel);
    o.wpos = mul(float4(i.pos, 1.0), gModel).xyz;
    o.col  = i.col;
    o.uv   = i.uv;
    o.wtan = mul(i.tan, (float3x3)gModel);
    return o;
}

// 프로브 볼륨 트라이리니어 샘플 (SH-L1) + 옥타헤드럴 depth Chebyshev 가시성 (누광 방지).
// 픽셀당 RT 레이 없이, 프로브의 사전계산 depth 통계로 소프트 가시성 가중 (DDGI 표준).
ProbeSH SampleProbes(float3 wpos, float3 N)
{
    int PX = (int)gGridDim.x, PY = (int)gGridDim.y, PZ = (int)gGridDim.z;
    float3 dimM1 = float3(PX - 1, PY - 1, PZ - 1);
    float3 bias = N * 0.04; // 자기그림자 방지 표면 바이어스
    float3 g = saturate((wpos + bias - gGridMin.xyz) / (gGridMax.xyz - gGridMin.xyz));
    float3 f = g * dimM1;
    int3 b0 = (int3)floor(f);
    float3 fr = f - (float3)b0;

    ProbeSH accV; accV.c0 = 0; accV.c1 = 0; accV.c2 = 0; accV.c3 = 0; // 가시성 가중
    ProbeSH accA; accA.c0 = 0; accA.c1 = 0; accA.c2 = 0; accA.c3 = 0; // 폴백(가중합=1)
    float wsumV = 0.0, wsumA = 0.0;

    [unroll] for (int s = 0; s < 8; ++s)
    {
        int3 o = int3(s & 1, (s >> 1) & 1, (s >> 2) & 1);
        int3 c = min(b0 + o, int3(PX - 1, PY - 1, PZ - 1));
        int idx = c.x + c.y * PX + c.z * PX * PY;
        float3 w3 = lerp(1.0 - fr, fr, (float3)o);
        float w = w3.x * w3.y * w3.z;
        ProbeSH p = gProbes[idx];

        float3 ppos = lerp(gGridMin.xyz, gGridMax.xyz, (float3)c / dimM1);
        float3 toSurf = wpos - ppos;       // 프로브 → 표면
        float dist = length(toSurf);
        float3 dir = (dist > 1e-4) ? toSurf / dist : N;

        // 법선 방향 가중 — 표면이 등진 프로브는 약화 (배면 누광 억제)
        float wnb = saturate(dot(-dir, N)) * 0.5 + 0.5;
        float wn = wnb * wnb + 0.05;

        // Chebyshev 가시성: 텍셀의 mean/mean² 로 분산 가시성 추정
        float2 uv = OctEncode(dir);
        int2 tc = clamp((int2)(uv * OCT), 0, OCT - 1);
        float2 md = gProbeDepth[idx * (OCT * OCT) + tc.y * OCT + tc.x];
        float vis = 1.0;
        if (dist > md.x)
        {
            float var = max(md.y - md.x * md.x, 1e-3);
            float d = dist - md.x;
            vis = var / (var + d * d);
            vis = vis * vis * vis; // 가장자리 강조 (DDGI 표준)
        }

        float wv = w * wn * saturate(vis);
        accV.c0 += p.c0 * wv; accV.c1 += p.c1 * wv; accV.c2 += p.c2 * wv; accV.c3 += p.c3 * wv;
        accA.c0 += p.c0 * w;  accA.c1 += p.c1 * w;  accA.c2 += p.c2 * w;  accA.c3 += p.c3 * w;
        wsumV += wv; wsumA += w;
    }

    if (wsumV > 1e-4)
    {
        float inv = 1.0 / wsumV;
        ProbeSH r; r.c0 = accV.c0 * inv; r.c1 = accV.c1 * inv; r.c2 = accV.c2 * inv; r.c3 = accV.c3 * inv;
        return r;
    }
    float inv = 1.0 / max(wsumA, 1e-4);
    ProbeSH r; r.c0 = accA.c0 * inv; r.c1 = accA.c1 * inv; r.c2 = accA.c2 * inv; r.c3 = accA.c3 * inv;
    return r; // 전부 가림 → 트라이리니어 폴백
}

// SH-L1 → 표면 법선 방향 irradiance (코사인 컨볼루션 상수 적용)
float3 EvalIrradiance(ProbeSH p, float3 N)
{
    float3 irr = 0.886227 * p.c0 + 1.023328 * (p.c1 * N.x + p.c2 * N.y + p.c3 * N.z);
    return max(irr, 0.0);
}

float4 PSMain(VSOut i) : SV_TARGET
{
    float3 Ngeo = normalize(i.wnrm);
    float3 N = Ngeo;

    // ── 노멀 매핑 (TBN, 모델만) — 탄젠트가 퇴화(0)면 지오메트릭 노멀 폴백(NaN 방지) ──
    if (gUseTex != 0)
    {
        float3 T = i.wtan - Ngeo * dot(i.wtan, Ngeo); // Gram-Schmidt
        if (dot(T, T) > 1e-5)
        {
            T = normalize(T);
            float3 B = cross(Ngeo, T);
            float3 nTS = gNormalMap.Sample(gSamp, i.uv).rgb * 2.0 - 1.0;
            N = normalize(nTS.x * T + nTS.y * B + nTS.z * Ngeo);
        }
    }

    float3 L = normalize(-gLightDir.xyz);
    float  ndl = saturate(dot(N, L));
    float3 V = normalize(gCamPos.xyz - i.wpos);

    // 머티리얼
    float metallic  = (gUseTex != 0) ? gMatParams.x : 0.0;
    float roughness = (gUseTex != 0) ? gMatParams.y : 0.6;
    float emissive  = (gUseTex != 0) ? gMatParams.z : 0.0;
    float3 albedo   = (gUseTex != 0) ? gDiffuse.Sample(gSamp, i.uv).rgb * gTint.rgb : i.col;
    float power = lerp(8.0, 256.0, 1.0 - roughness);
    float3 specColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float specMask = (gUseTex != 0) ? gSpecMap.Sample(gSamp, i.uv).r : 1.0;

    // ── 디버그 뷰 (1 albedo / 2 normal / 3 depth) ──
    int dv = (int)gDebug.x;
    if (dv == 1) return float4(albedo, 1);
    if (dv == 2) return float4(N * 0.5 + 0.5, 1);
    if (dv == 3) { float dd = saturate(distance(gCamPos.xyz, i.wpos) / 40.0); return float4(dd, dd, dd, 1); }

    // ── 방향광 RT 그림자 (소프트: gSkyZenith.w) ──
    float shadow = 1.0;
    if (ndl > 0.0)
    {
        float soft = gSkyZenith.w;
        int K = (soft > 0.001) ? 4 : 1;
        float occ = 0.0;
        [unroll] for (int s = 0; s < 4; ++s)
        {
            if (s >= K) break;
            float3 Ls = L;
            if (K > 1) { float a = float(s) * 1.7 + i.wpos.x * 11.3 + i.wpos.z * 7.1; Ls = normalize(L + float3(cos(a), sin(a * 1.3), sin(a)) * soft); }
            RayDesc ray; ray.Origin = i.wpos + Ngeo * 0.02; ray.Direction = Ls; ray.TMin = 0.001; ray.TMax = 100.0;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> q;
            q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, ray); q.Proceed();
            if (q.CommittedStatus() != COMMITTED_NOTHING) occ += 1.0;
        }
        shadow = 1.0 - occ / float(K);
    }

    float3 sunCol = gSunColor.rgb * gLightDir.w;
    float3 direct = albedo * (ndl * shadow) * sunCol * (1.0 - metallic * 0.75);
    float3 spec = 0;
    if (ndl > 0.0) { float3 H = normalize(L + V); spec += pow(saturate(dot(N, H)), power) * specMask * shadow * sunCol * specColor * (1.0 - roughness * 0.5); }

    // ── 다중 점광원 ──
    [unroll] for (int p = 0; p < 4; ++p)
    {
        if (gPtCol[p].w < 0.5) continue;
        float3 toL = gPtPos[p].xyz - i.wpos; float d = length(toL);
        if (d >= gPtPos[p].w || d <= 1e-4) continue;
        float3 Lp = toL / d; float att = saturate(1.0 - d / gPtPos[p].w); att *= att;
        float ndlp = saturate(dot(N, Lp));
        float psh = 1.0;
        if (ndlp > 0.0) { RayDesc r; r.Origin = i.wpos + Ngeo * 0.02; r.Direction = Lp; r.TMin = 0.001; r.TMax = d - 0.05;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> q; q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, r); q.Proceed();
            if (q.CommittedStatus() != COMMITTED_NOTHING) psh = 0.0; }
        float3 pc = gPtCol[p].rgb * (att * psh);
        direct += albedo * ndlp * pc * (1.0 - metallic * 0.75);
        float3 Hp = normalize(Lp + V); spec += pow(saturate(dot(N, Hp)), power) * ndlp * pc * specColor;
    }

    // ── 스팟 라이트 (콘 + 가장자리 페이드 + RT 그림자) ──
    if (gSpotColor.w > 0.5)
    {
        float3 toL = gSpotPos.xyz - i.wpos; float d = length(toL);
        if (d < gSpotPos.w && d > 1e-4)
        {
            float3 Lp = toL / d;
            float cone = dot(-Lp, gSpotDir.xyz);
            if (cone > gSpotDir.w)
            {
                float att = saturate(1.0 - d / gSpotPos.w); att *= att;
                float edge = saturate((cone - gSpotDir.w) / (1.0 - gSpotDir.w));
                float ndlp = saturate(dot(N, Lp));
                float psh = 1.0;
                if (ndlp > 0.0) { RayDesc r; r.Origin = i.wpos + Ngeo * 0.02; r.Direction = Lp; r.TMin = 0.001; r.TMax = d - 0.05;
                    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> q; q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, r); q.Proceed();
                    if (q.CommittedStatus() != COMMITTED_NOTHING) psh = 0.0; }
                float3 pc = gSpotColor.rgb * (att * edge * psh);
                direct += albedo * ndlp * pc * (1.0 - metallic * 0.75);
                float3 Hp = normalize(Lp + V); spec += pow(saturate(dot(N, Hp)), power) * ndlp * pc * specColor;
            }
        }
    }

    ProbeSH shp     = SampleProbes(i.wpos, N);
    float3 indirect = albedo * EvalIrradiance(shp, N) * gGI.x * gSunColor.w; // 환경강도(gSunColor.w)
    if (dv == 4) return float4(indirect, 1);

    float3 col = albedo * gGI.z * gSunColor.w + direct + indirect + spec;
    col += albedo * emissive;

    // ── RT 반사 (글로시 비금속/바닥, gDebug.w = strength) ──
    float reflStr = gDebug.w;
    if (reflStr > 0.001)
    {
        float3 R = reflect(-V, N);
        RayDesc r; r.Origin = i.wpos + Ngeo * 0.02; r.Direction = R; r.TMin = 0.02; r.TMax = 60.0;
        RayQuery<RAY_FLAG_CULL_NON_OPAQUE> q; q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, r); q.Proceed();
        float3 refl;
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        { float3 hp = r.Origin + R * q.CommittedRayT(); ProbeSH hs = SampleProbes(hp, R); refl = EvalIrradiance(hs, R); }
        else { float t = saturate(R.y); refl = lerp(gSkyHorizon.rgb, gSkyZenith.rgb, pow(t, 0.55)); }
        float fres = pow(1.0 - saturate(dot(N, V)), 4.0);
        col = lerp(col, refl, saturate(reflStr * (0.15 + 0.85 * fres)));
    }

    // ── 거리 안개 ──
    if (gFog.w > 1e-5)
    {
        float fogF = 1.0 - exp(-distance(gCamPos.xyz, i.wpos) * gFog.w);
        col = lerp(col, gFog.rgb, saturate(fogF));
    }
    return float4(col, 1.0);
}
)";

// DDGI 프로브 수집 — 컴퓨트에서 프로브마다 RT 레이를 구면 방향으로 쏘아
// 1차 직접광(히트 표면의 albedo×N·L)을 평균 → DC irradiance 로 저장.
static const std::string kGatherShader = std::string(kSceneCB) + R"(
#define OCT 8        // 프로브당 옥타헤드럴 depth 해상도 (8×8=64 텍셀)
#define RAY_CAP 128  // hitDist 로컬 배열 상한 (= gGridDim.w)
struct Vtx { float3 pos; float3 nrm; float3 col; float2 uv; float3 tan; }; // C++ Vtx 와 동일 레이아웃
struct ProbeSH { float3 c0; float3 c1; float3 c2; float3 c3; };

RaytracingAccelerationStructure gScene   : register(t0);
StructuredBuffer<Vtx>           gVerts    : register(t1);
StructuredBuffer<uint>          gIndices  : register(t2);
RWStructuredBuffer<ProbeSH>     gProbesRW : register(u0);
RWStructuredBuffer<float2>      gDepthRW  : register(u1); // 옥타헤드럴 depth (mean, mean²)

// 구면 균일 분포 (피보나치)
float3 FibDir(uint i, uint n)
{
    float k = i + 0.5;
    float phi = 6.2831853 * frac(k * 0.61803399);
    float cosT = 1.0 - 2.0 * k / n;
    float sinT = sqrt(saturate(1.0 - cosT * cosT));
    return float3(cos(phi) * sinT, cosT, sin(phi) * sinT);
}

// 옥타헤드럴 매핑: 단위벡터 ↔ [0,1]² (프로브 depth 의 방향 인덱싱)
float3 OctDecode(float2 f)
{
    f = f * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// 이전 프레임 프로브 볼륨에서 irradiance 읽기 (다중 바운스용 — gProbesRW 트라이리니어 SH)
float3 ProbeIrradiancePrev(float3 wpos, float3 N)
{
    int PX = (int)gGridDim.x, PY = (int)gGridDim.y, PZ = (int)gGridDim.z;
    float3 g = saturate((wpos - gGridMin.xyz) / (gGridMax.xyz - gGridMin.xyz));
    float3 f = g * float3(PX - 1, PY - 1, PZ - 1);
    int3 b0 = (int3)floor(f); float3 fr = f - (float3)b0;
    float3 c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    [unroll] for (int s = 0; s < 8; ++s)
    {
        int3 o = int3(s & 1, (s >> 1) & 1, (s >> 2) & 1);
        int3 c = min(b0 + o, int3(PX - 1, PY - 1, PZ - 1));
        int idx = c.x + c.y * PX + c.z * PX * PY;
        float3 w3 = lerp(1.0 - fr, fr, (float3)o); float w = w3.x * w3.y * w3.z;
        ProbeSH p = gProbesRW[idx];
        c0 += p.c0 * w; c1 += p.c1 * w; c2 += p.c2 * w; c3 += p.c3 * w;
    }
    float3 irr = 0.886227 * c0 + 1.023328 * (c1 * N.x + c2 * N.y + c3 * N.z);
    return max(irr, 0.0);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint PX = (uint)gGridDim.x, PY = (uint)gGridDim.y, PZ = (uint)gGridDim.z;
    uint pi = tid.x;
    if (pi >= PX * PY * PZ) return;

    uint px = pi % PX, py = (pi / PX) % PY, pz = pi / (PX * PY);
    float3 t = float3(px / max(PX - 1.0, 1.0), py / max(PY - 1.0, 1.0), pz / max(PZ - 1.0, 1.0));
    float3 ppos = lerp(gGridMin.xyz, gGridMax.xyz, t);

    uint K = min((uint)gGridDim.w, (uint)RAY_CAP);
    float3 L = normalize(-gLightDir.xyz);
    const float MAXD = 40.0;

    // 입사휘도를 SH-L1 로 투영 (구면 균일 샘플) + 히트 거리 저장(옥타헤드럴 depth 용)
    float3 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    float hitDist[RAY_CAP];
    for (uint r = 0; r < K; ++r)
    {
        float3 dir = FibDir(r, K);
        RayDesc ray; ray.Origin = ppos; ray.Direction = dir; ray.TMin = 0.02; ray.TMax = MAXD;
        RayQuery<RAY_FLAG_CULL_NON_OPAQUE> q;
        q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, ray);
        q.Proceed();

        float3 rad;
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            uint prim = q.CommittedPrimitiveIndex();
            float2 b = q.CommittedTriangleBarycentrics();
            uint i0 = gIndices[prim * 3 + 0], i1 = gIndices[prim * 3 + 1], i2 = gIndices[prim * 3 + 2];
            Vtx v0 = gVerts[i0], v1 = gVerts[i1], v2 = gVerts[i2];
            float w0 = 1.0 - b.x - b.y;
            float3 n = normalize(v0.nrm * w0 + v1.nrm * b.x + v2.nrm * b.y);
            float3 alb = v0.col * w0 + v1.col * b.x + v2.col * b.y;
            float rt = q.CommittedRayT();
            float3 hitPos = ppos + dir * rt;
            // 직접광 + 이전 프레임 간접광(다중 바운스) — 프레임마다 한 바운스씩 전파
            float3 lit = saturate(dot(n, L)) * gLightDir.w + ProbeIrradiancePrev(hitPos, n) * gGI.x;
            rad = alb * lit;
            // 뒷면(프로브가 벽 안쪽) 히트는 음수 거리로 기록 → depth 가 가까이 잡혀 누광 차단
            hitDist[r] = (dot(n, dir) > 0.0) ? -rt * 0.2 : rt;
        }
        else
        {
            rad = float3(0.10, 0.12, 0.16); // 하늘
            hitDist[r] = MAXD;
        }
        // SH-L1 기저 (Y00, Y1m1·y, Y10·z, Y11·x)
        s0 += rad * 0.282095;
        s1 += rad * 0.488603 * dir.x;
        s2 += rad * 0.488603 * dir.y;
        s3 += rad * 0.488603 * dir.z;
    }
    float wmc = (4.0 * 3.14159265 / K); // 구면 균일 몬테카를로 가중
    ProbeSH cur;
    cur.c0 = s0 * wmc; cur.c1 = s1 * wmc; cur.c2 = s2 * wmc; cur.c3 = s3 * wmc;

    // 시간적 누적 (EMA) — 첫 프레임은 즉시 대입, 이후 부드럽게 블렌드
    float alpha = (gGI.y < 2.0) ? 1.0 : 0.08;
    ProbeSH prev = gProbesRW[pi];
    ProbeSH outp;
    outp.c0 = lerp(prev.c0, cur.c0, alpha);
    outp.c1 = lerp(prev.c1, cur.c1, alpha);
    outp.c2 = lerp(prev.c2, cur.c2, alpha);
    outp.c3 = lerp(prev.c3, cur.c3, alpha);
    gProbesRW[pi] = outp;

    // ── 옥타헤드럴 depth: 텍셀 방향에 가까운 레이 거리를 가중 평균(mean, mean²) ──
    // Chebyshev(분산) 가시성을 위해 mean/mean² 저장. 텍셀당 모든 레이를 sharp 가중 합산.
    const float sharp = 12.0;
    for (uint tj = 0; tj < OCT * OCT; ++tj)
    {
        float2 tuv = (float2(tj % OCT, tj / OCT) + 0.5) / OCT;
        float3 tdir = OctDecode(tuv);
        float sw = 0.0, sd = 0.0, sd2 = 0.0;
        for (uint r = 0; r < K; ++r)
        {
            float wgt = pow(max(0.0, dot(tdir, FibDir(r, K))), sharp);
            float d = hitDist[r];
            sw += wgt; sd += wgt * d; sd2 += wgt * d * d;
        }
        float2 md = (sw > 1e-6) ? float2(sd / sw, sd2 / sw) : float2(MAXD, MAXD * MAXD);
        uint di = pi * (OCT * OCT) + tj;
        gDepthRW[di] = lerp(gDepthRW[di], md, alpha);
    }
}
)";

// 절차적 스카이박스 — 풀스크린 삼각형, invVP 로 월드 레이 복원 → 그라데이션 + 태양
static const std::string kSkyShader = std::string(kSceneCB) + R"(
struct VOut { float4 pos:SV_POSITION; float2 clip:TEXCOORD0; };
VOut VSMain(uint id : SV_VertexID)
{
    VOut o;
    float2 p = float2((id << 1) & 2, id & 2); // (0,0)(2,0)(0,2)
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.clip = o.pos.xy;
    return o;
}
float4 PSMain(VOut i) : SV_TARGET
{
    float4 wn = mul(float4(i.clip, 0.0, 1.0), gInvVP); wn /= wn.w;
    float4 wf = mul(float4(i.clip, 1.0, 1.0), gInvVP); wf /= wf.w;
    float3 dir = normalize(wf.xyz - wn.xyz);

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
    return float4(sky, 1.0);
}
)";

// 무한 씬 그리드 — 풀스크린 레이를 y=0 평면과 교차, 월드 그리드 라인(1m/10m) + 축 색 + 거리 페이드.
// SV_Depth 로 지오메트리에 가려지게(깊이 테스트), 알파 블렌드.
static const std::string kGridShader = std::string(kSceneCB) + R"(
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

    float a = max(GridA(wp.xz, 1.0) * 0.30, GridA(wp.xz, 10.0) * 0.65);
    float3 col = float3(0.55, 0.55, 0.62);
    float ax = 1.0 - min(abs(wp.z) / fwidth(wp.z), 1.0); // z≈0 → X축(빨강)
    float az = 1.0 - min(abs(wp.x) / fwidth(wp.x), 1.0); // x≈0 → Z축(파랑)
    if (ax > 0.1) { col = float3(0.9, 0.25, 0.25); a = max(a, ax); }
    if (az > 0.1) { col = float3(0.25, 0.45, 0.95); a = max(a, az); }
    a *= saturate(1.0 - length(wp.xz - ro.xz) / 60.0);
    if (a <= 0.001) discard;
    o.col = float4(col, a);
    return o;
}
)";

// 선택 아웃라인 — 인버티드 헐 (법선 방향 팽창, 앞면 컬링 → 가장자리 림)
static const std::string kOutlineShader = std::string(kSceneCB) + R"(
struct VIn { float3 pos:POSITION; float3 nrm:NORMAL; float3 col:COLOR; float2 uv:TEXCOORD; float3 tan:TANGENT; };
float4 VSMain(VIn i) : SV_POSITION
{
    float3 wp = i.pos;                 // 정점은 이미 월드(스키닝/기즈모 반영)
    float3 n = normalize(i.nrm);
    float d = distance(gCamPos.xyz, wp);
    wp += n * d * 0.005;               // 화면상 일정 두께 (얇게)
    return mul(float4(wp, 1.0), gMVP);
}
float4 PSMain() : SV_TARGET { return float4(1.7, 0.85, 0.12, 1.0); } // HDR 주황 (톤맵 후에도 선명)
)";

// 포스트프로세스 공용 — 풀스크린 삼각형 VS
static const char* kPostCommon = R"(
struct VOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
VOut VSFull(uint id : SV_VertexID)
{
    VOut o; float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv; o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); return o;
}
)";
// ACES 톤맵 + 노출 + 감마 (+ S4 블룸 합성)
static const std::string kTonemapShader = std::string(kPostCommon) + R"(
Texture2D gHDR : register(t0);
Texture2D gBloom : register(t1);
SamplerState gS : register(s0);
cbuffer PostCB : register(b0) { float gExposure; float gBloomI; float gBloomOn; float gTonemapOp;
                                float gContrast; float gSaturation; float gTemperature; float gVignette; };
float3 ACES(float3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14; return saturate((x*(a*x+b))/(x*(c*x+d)+e)); }
float3 Filmic(float3 x){ x=max(0,x-0.004); return (x*(6.2*x+0.5))/(x*(6.2*x+1.7)+0.06); } // 감마 내장
float4 PSTonemap(VOut i) : SV_TARGET
{
    float3 hdr = gHDR.Sample(gS, i.uv).rgb;
    if (gBloomOn > 0.5) hdr += gBloom.Sample(gS, i.uv).rgb * gBloomI;
    hdr *= gExposure;
    float3 c;
    if (gTonemapOp < 0.5)      { c = pow(ACES(hdr), 1.0/2.2); }
    else if (gTonemapOp < 1.5) { c = pow(hdr / (1.0 + hdr), 1.0/2.2); } // Reinhard
    else                       { c = Filmic(hdr); }                     // Filmic(감마 포함)
    // 컬러 그레이딩
    c.r *= 1.0 + gTemperature * 0.12; c.b *= 1.0 - gTemperature * 0.12;  // 색온도
    c = saturate((c - 0.5) * gContrast + 0.5);                          // 대비
    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, gSaturation);                 // 채도
    // 비네트
    float2 dd = i.uv - 0.5;
    c *= saturate(1.0 - dot(dd, dd) * gVignette * 2.8);
    return float4(c, 1.0);
}
// 브라이트패스 — 휘도 임계값(gExposure 재사용) 초과분 추출
float4 PSBright(VOut i) : SV_TARGET
{
    float3 c = gHDR.Sample(gS, i.uv).rgb;
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float contrib = max(l - gExposure, 0.0);
    return float4(c * (contrib / (l + 1e-4)), 1.0);
}
// 분리형 가우시안 (cbuffer 재해석: gExposure,gBloomI=texel / gBloomOn,gTonemapOp=방향)
float4 PSBlur(VOut i) : SV_TARGET
{
    float2 texel = float2(gExposure, gBloomI);
    float2 dir   = float2(gBloomOn, gTonemapOp);
    float w[5] = { 0.227027, 0.194594, 0.121622, 0.054054, 0.016216 };
    float3 sum = gHDR.Sample(gS, i.uv).rgb * w[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 o = dir * texel * (float)k;
        sum += gHDR.Sample(gS, i.uv + o).rgb * w[k];
        sum += gHDR.Sample(gS, i.uv - o).rgb * w[k];
    }
    return float4(sum, 1.0);
}
)";

ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target); // 전방 선언

void D3D12Device::CreatePostFX()
{
	// 공용 SRV 힙 (shader-visible)
	D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 8;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_postSrvHeap)), "post srv heap");
	_postSrvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 루트시그: t0..t1 테이블 + b0 루트상수(4) + s0
	D3D12_DESCRIPTOR_RANGE range{}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 2; range.BaseShaderRegister = 0; range.OffsetInDescriptorsFromTableStart = 0;
	D3D12_ROOT_PARAMETER p[2]{};
	p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	p[0].DescriptorTable.NumDescriptorRanges = 1; p[0].DescriptorTable.pDescriptorRanges = &range;
	p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p[1].Constants.ShaderRegister = 0; p[1].Constants.Num32BitValues = 8;
	p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_STATIC_SAMPLER_DESC s{}; s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; s.ShaderRegister = 0;
	s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; s.MaxLOD = D3D12_FLOAT32_MAX;
	D3D12_ROOT_SIGNATURE_DESC rs{}; rs.NumParameters = 2; rs.pParameters = p; rs.NumStaticSamplers = 1; rs.pStaticSamplers = &s;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "post rootsig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_postRootSig)), "post rootsig create");

	auto makePSO = [&](const std::string& shader, const wchar_t* psEntry, DXGI_FORMAT fmt, ComPtr<ID3D12PipelineState>& out)
	{
		ComPtr<IDxcBlob> vs = CompileDxc(shader.c_str(), L"VSFull", L"vs_6_5");
		ComPtr<IDxcBlob> ps = CompileDxc(shader.c_str(), psEntry, L"ps_6_5");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
		d.pRootSignature = _postRootSig.Get();
		d.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		d.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		d.DepthStencilState.DepthEnable = FALSE;
		d.SampleMask = UINT_MAX; d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		d.NumRenderTargets = 1; d.RTVFormats[0] = fmt; d.SampleDesc.Count = 1;
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&out)), "post pso");
	};
	makePSO(kTonemapShader, L"PSTonemap", DXGI_FORMAT_R8G8B8A8_UNORM, _tonemapPSO);
	makePSO(kTonemapShader, L"PSBright", DXGI_FORMAT_R16G16B16A16_FLOAT, _brightPSO);
	makePSO(kTonemapShader, L"PSBlur",   DXGI_FORMAT_R16G16B16A16_FLOAT, _blurPSO);
}

// ───────────────────────────────────────────────────────────
void D3D12Device::Init(HWND hwnd, UINT width, UINT height)
{
	_width = width;
	_height = height;
	_hwnd = hwnd;

	CoInitializeEx(nullptr, COINIT_MULTITHREADED); // WIC 텍스처 로딩용

	EnableDebugLayer();
	CreateDeviceAndQueue();
	CreateSwapChain(hwnd);
	CreateRtvHeapAndTargets();
	CreateFrameResources();

	// Phase 1
	CreateDepthBuffer();
	CreateRootSignature();
	CreatePipeline();
	CreateConstantBuffers();

	// 모델(기본 Archer) 로드 — 메시 + 바닥 + 텍스처 + BLAS/TLAS (런타임 교체 가능)
	{
		wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
		LoadModelFromFile(dir + L"\\..\\Resources\\Assets\\Models\\Archer\\Archer.mesh");
	}

	// Phase 3 — DDGI 프로브 볼륨
	CreateGI();

	// 포스트프로세스 (HDR 톤맵 / 블룸) — SceneRT SRV 생성 전에 힙/PSO 준비
	CreatePostFX();

	// 에디터 UI (ImGui DX12 + 도킹)
	InitEditor();
}

void D3D12Device::EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		debug->EnableDebugLayer();
#endif
}

void D3D12Device::CreateDeviceAndQueue()
{
	UINT factoryFlags = 0;
#if defined(_DEBUG)
	factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&_factory)), "CreateDXGIFactory2");

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0;
	     _factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
	     ++i)
	{
		DXGI_ADAPTER_DESC1 desc{};
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device))))
		{
			_adapterName = desc.Description;
			break;
		}
		adapter.Reset();
	}
	if (!_device)
		ThrowIfFailed(E_FAIL, "No D3D12 12_1 adapter found");

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5))))
		_dxrTier = opt5.RaytracingTier;

	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&_queue)), "CreateCommandQueue");
}

void D3D12Device::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 scd{};
	scd.BufferCount = FRAME_COUNT;
	scd.Width = _width;
	scd.Height = _height;
	scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> sc1;
	ThrowIfFailed(_factory->CreateSwapChainForHwnd(_queue.Get(), hwnd, &scd, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd");
	_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	ThrowIfFailed(sc1.As(&_swapChain), "SwapChain As 3");
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::CreateRtvHeapAndTargets()
{
	D3D12_DESCRIPTOR_HEAP_DESC rd{};
	rd.NumDescriptors = FRAME_COUNT;
	rd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&_rtvHeap)), "CreateDescriptorHeap RTV");
	_rtvDescSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargets[i])), "GetBuffer");
		_device->CreateRenderTargetView(_renderTargets[i].Get(), nullptr, rtv);
		rtv.ptr += _rtvDescSize;
	}
}

void D3D12Device::CreateFrameResources()
{
	for (UINT i = 0; i < FRAME_COUNT; ++i)
		ThrowIfFailed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_allocators[i])), "CreateCommandAllocator");

	ThrowIfFailed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _allocators[_frameIndex].Get(), nullptr, IID_PPV_ARGS(&_cmdList)), "CreateCommandList");
	ThrowIfFailed(_cmdList->Close(), "CmdList Close");

	ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)), "CreateFence");
	_fenceValues[_frameIndex] = 1;
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
}

// ─── Phase 1 ───
void D3D12Device::CreateDepthBuffer()
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = _width;
	rd.Height = _height;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_D32_FLOAT;
	rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv{};
	cv.Format = DXGI_FORMAT_D32_FLOAT;
	cv.DepthStencil.Depth = 1.0f;

	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&_depth)), "Create Depth");

	D3D12_DESCRIPTOR_HEAP_DESC dd{};
	dd.NumDescriptors = 1;
	dd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&dd, IID_PPV_ARGS(&_dsvHeap)), "CreateDescriptorHeap DSV");

	_device->CreateDepthStencilView(_depth.Get(), nullptr, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Device::CreateRootSignature()
{
	// 디퓨즈 텍스처(t2) 디스크립터 테이블 범위
	D3D12_DESCRIPTOR_RANGE texRange{};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.NumDescriptors = 3; // t2 디퓨즈, t3 노멀, t4 스펙큘러
	texRange.BaseShaderRegister = 2; // t2
	texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 루트: 0=CBV(b0), 1=SRV(t0)TLAS, 2=SRV(t1)프로브, 3=테이블(t2 텍스처), 4=루트상수(b1 useTex), 5=SRV(t5)프로브 depth
	D3D12_ROOT_PARAMETER params[6]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // TLAS
	params[1].Descriptor.ShaderRegister = 0; // t0
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // 프로브
	params[2].Descriptor.ShaderRegister = 1; // t1
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &texRange;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[4].Constants.ShaderRegister = 1; // b1
	params[4].Constants.Num32BitValues = 1;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // 프로브 depth
	params[5].Descriptor.ShaderRegister = 5; // t5
	params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 정적 샘플러 s0 (선형 WRAP)
	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	samp.ShaderRegister = 0; // s0
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 6;
	rs.pParameters = params;
	rs.NumStaticSamplers = 1;
	rs.pStaticSamplers = &samp;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "SerializeRootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "CreateRootSig");
}

// DXC 컴파일 (SM6.x) — RayQuery(인라인 RT, SM6.5) 때문에 FXC 대신 DXC 사용.
// row_major 키워드로 DirectXMath(행우선) 일치는 셰이더 소스에서 명시.
ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target)
{
	static ComPtr<IDxcUtils> utils;
	static ComPtr<IDxcCompiler3> compiler;
	if (!utils) ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DxcCreateInstance Utils");
	if (!compiler) ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), "DxcCreateInstance Compiler");

	DxcBuffer srcBuf{};
	srcBuf.Ptr = src;
	srcBuf.Size = strlen(src);
	srcBuf.Encoding = DXC_CP_UTF8;

	std::vector<LPCWSTR> args = { L"-E", entry, L"-T", target, L"-HV", L"2021" };
#if defined(_DEBUG)
	args.push_back(L"-Zi");
	args.push_back(L"-Qembed_debug");
	args.push_back(L"-Od");
#endif

	ComPtr<IDxcResult> result;
	ThrowIfFailed(compiler->Compile(&srcBuf, args.data(), (UINT)args.size(), nullptr, IID_PPV_ARGS(&result)), "DXC Compile");

	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
		OutputDebugStringA(errors->GetStringPointer());

	HRESULT status = S_OK;
	result->GetStatus(&status);
	ThrowIfFailed(status, "DXC shader has errors");

	ComPtr<IDxcBlob> code;
	ThrowIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&code), nullptr), "DXC GetOutput");
	return code;
}

void D3D12Device::CreatePipeline()
{
	ComPtr<IDxcBlob> vs = CompileDxc(kMeshShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ps = CompileDxc(kMeshShader.c_str(), L"PSMain", L"ps_6_5");

	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rast{};
	rast.FillMode = D3D12_FILL_MODE_SOLID;
	rast.CullMode = D3D12_CULL_MODE_NONE; // Phase 1: 안팎 모두 보이게
	rast.FrontCounterClockwise = FALSE;
	rast.DepthClipEnable = TRUE;

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC ds{};
	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig.Get();
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.InputLayout = { layout, _countof(layout) };
	pso.RasterizerState = rast;
	pso.BlendState = blend;
	pso.DepthStencilState = ds;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = _sceneFmt;
	pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso.SampleDesc.Count = 1;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "CreatePSO");

	// ── 와이어프레임 PSO (모델 토글) ──
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wpso = pso;
	wpso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&wpso, IID_PPV_ARGS(&_wirePSO)), "CreateWirePSO");

	// ── 스카이박스 PSO (풀스크린 삼각형, 깊이/입력레이아웃 없음, b0 만 사용) ──
	ComPtr<IDxcBlob> svs = CompileDxc(kSkyShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> sps = CompileDxc(kSkyShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC spso{};
	spso.pRootSignature = _rootSig.Get();
	spso.VS = { svs->GetBufferPointer(), svs->GetBufferSize() };
	spso.PS = { sps->GetBufferPointer(), sps->GetBufferSize() };
	spso.RasterizerState = rast;
	spso.BlendState = blend;
	spso.DepthStencilState.DepthEnable = FALSE;   // 배경 — 깊이 테스트/쓰기 없음
	spso.SampleMask = UINT_MAX;
	spso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	spso.NumRenderTargets = 1;
	spso.RTVFormats[0] = _sceneFmt;
	spso.DSVFormat = DXGI_FORMAT_UNKNOWN;
	spso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&spso, IID_PPV_ARGS(&_skyPSO)), "CreateSkyPSO");

	// ── 그리드 PSO (깊이 테스트 LESS_EQUAL/쓰기 없음, 알파 블렌드, SV_Depth 출력) ──
	ComPtr<IDxcBlob> gvs = CompileDxc(kGridShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> gps = CompileDxc(kGridShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpso = spso;
	gpso.VS = { gvs->GetBufferPointer(), gvs->GetBufferSize() };
	gpso.PS = { gps->GetBufferPointer(), gps->GetBufferSize() };
	gpso.DepthStencilState.DepthEnable = TRUE;
	gpso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	gpso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	gpso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	gpso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	gpso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	gpso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	gpso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gpso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&gpso, IID_PPV_ARGS(&_gridPSO)), "CreateGridPSO");

	// ── 아웃라인 PSO (앞면 컬링 = 뒷면 렌더, 깊이 LESS/쓰기, 입력레이아웃 = 메시) ──
	ComPtr<IDxcBlob> ovs = CompileDxc(kOutlineShader.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ops = CompileDxc(kOutlineShader.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opso{};
	opso.pRootSignature = _rootSig.Get();
	opso.VS = { ovs->GetBufferPointer(), ovs->GetBufferSize() };
	opso.PS = { ops->GetBufferPointer(), ops->GetBufferSize() };
	opso.InputLayout = { layout, _countof(layout) };
	opso.RasterizerState = rast; opso.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	opso.BlendState = blend;
	opso.DepthStencilState = ds; // LESS, write on
	opso.SampleMask = UINT_MAX;
	opso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opso.NumRenderTargets = 1; opso.RTVFormats[0] = _sceneFmt;
	opso.DSVFormat = DXGI_FORMAT_D32_FLOAT; opso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&opso, IID_PPV_ARGS(&_outlinePSO)), "CreateOutlinePSO");
}

ComPtr<ID3D12Resource> D3D12Device::CreateUploadBuffer(const void* data, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "CreateUploadBuffer");

	if (data)
	{
		void* p = nullptr;
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(buf->Map(0, &noRead, &p), "Map upload");
		memcpy(p, data, size);
		buf->Unmap(0, nullptr);
	}
	return buf;
}


ComPtr<ID3D12Resource> D3D12Device::CreateDefaultBuffer(UINT64 size, D3D12_RESOURCE_STATES state, bool allowUAV)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, nullptr, IID_PPV_ARGS(&buf)), "CreateDefaultBuffer");
	return buf;
}

// BLAS 입력(삼각형 지오메트리) 구성 — 매번 동일(정점 데이터만 GPU 실행 시 갱신)
static void FillBlasGeom(ID3D12Resource* vb, ID3D12Resource* ib, UINT vcount, UINT icount,
                         D3D12_RAYTRACING_GEOMETRY_DESC& geom)
{
	geom = {};
	geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geom.Triangles.VertexBuffer.StartAddress = vb->GetGPUVirtualAddress();
	geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vtx);
	geom.Triangles.VertexCount = vcount;
	geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geom.Triangles.IndexBuffer = ib->GetGPUVirtualAddress();
	geom.Triangles.IndexCount = icount;
	geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
}

void D3D12Device::CreateASBuffers()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geom{};
	FillBlasGeom(_vb.Get(), _ib.Get(), _vertexCount, _indexCount, geom);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasIn{};
	blasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	blasIn.NumDescs = 1;
	blasIn.pGeometryDescs = &geom;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo{};
	_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasIn, &blasInfo);
	_blas        = CreateDefaultBuffer(blasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_blasScratch = CreateDefaultBuffer(blasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	D3D12_RAYTRACING_INSTANCE_DESC inst{};
	inst.Transform[0][0] = 1.f; inst.Transform[1][1] = 1.f; inst.Transform[2][2] = 1.f;
	inst.InstanceMask = 0xFF;
	inst.AccelerationStructure = _blas->GetGPUVirtualAddress();
	_instanceBuffer = CreateUploadBuffer(&inst, sizeof(inst));

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasIn{};
	tlasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	tlasIn.NumDescs = 1;
	tlasIn.InstanceDescs = _instanceBuffer->GetGPUVirtualAddress();
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo{};
	_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasIn, &tlasInfo);
	_tlas        = CreateDefaultBuffer(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_tlasScratch = CreateDefaultBuffer(tlasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// 초기 빌드 (1회) — 정적이면 이것만으로 충분, 스키닝이면 매 프레임 RecordBuildAS 재빌드
	ThrowIfFailed(_allocators[_frameIndex]->Reset(), "AS alloc reset");
	ThrowIfFailed(_cmdList->Reset(_allocators[_frameIndex].Get(), nullptr), "AS cmd reset");
	RecordBuildAS();
	ThrowIfFailed(_cmdList->Close(), "AS cmd close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();
}

void D3D12Device::RecordBuildAS()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geom{};
	FillBlasGeom(_vb.Get(), _ib.Get(), _vertexCount, _indexCount, geom);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasIn{};
	blasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	blasIn.NumDescs = 1;
	blasIn.pGeometryDescs = &geom;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuild{};
	blasBuild.Inputs = blasIn;
	blasBuild.DestAccelerationStructureData = _blas->GetGPUVirtualAddress();
	blasBuild.ScratchAccelerationStructureData = _blasScratch->GetGPUVirtualAddress();
	_cmdList->BuildRaytracingAccelerationStructure(&blasBuild, 0, nullptr);

	D3D12_RESOURCE_BARRIER ub{};
	ub.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	ub.UAV.pResource = _blas.Get();
	_cmdList->ResourceBarrier(1, &ub);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasIn{};
	tlasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	tlasIn.NumDescs = 1;
	tlasIn.InstanceDescs = _instanceBuffer->GetGPUVirtualAddress();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
	tlasBuild.Inputs = tlasIn;
	tlasBuild.DestAccelerationStructureData = _tlas->GetGPUVirtualAddress();
	tlasBuild.ScratchAccelerationStructureData = _tlasScratch->GetGPUVirtualAddress();
	_cmdList->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);

	ub.UAV.pResource = _tlas.Get();
	_cmdList->ResourceBarrier(1, &ub);
}

void D3D12Device::Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to)
{
	if (cur == to) return;
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.StateBefore = cur;
	b.Transition.StateAfter = to;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &b);
	cur = to;
}

void D3D12Device::CreateGI()
{
	// 프로브 버퍼 (ProbeSH = float3 × 4 = SH-L1) × PROBE_COUNT — 컴퓨트 UAV 쓰기 / 픽셀 SRV 읽기
	_probes = CreateDefaultBuffer(PROBE_COUNT * sizeof(XMFLOAT3) * 4, D3D12_RESOURCE_STATE_COMMON, true);
	_probeState = D3D12_RESOURCE_STATE_COMMON;

	// 옥타헤드럴 depth 버퍼 (float2 mean/mean²) × PROBE_COUNT × OCT²
	_probeDepth = CreateDefaultBuffer(PROBE_COUNT * PROBE_OCT * PROBE_OCT * sizeof(XMFLOAT2), D3D12_RESOURCE_STATE_COMMON, true);
	_probeDepthState = D3D12_RESOURCE_STATE_COMMON;

	// 컴퓨트 루트 시그니처: b0 CBV, u0 probes, u1 depth, t0 TLAS, t1 verts, t2 indices (전부 루트 디스크립터)
	D3D12_ROOT_PARAMETER p[6]{};
	p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; p[0].Descriptor.ShaderRegister = 0;
	p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[1].Descriptor.ShaderRegister = 0;
	p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[2].Descriptor.ShaderRegister = 1; // depth
	p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[3].Descriptor.ShaderRegister = 0;
	p[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[4].Descriptor.ShaderRegister = 1;
	p[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[5].Descriptor.ShaderRegister = 2;
	for (auto& rp : p) rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 6;
	rs.pParameters = p;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "Serialize GI RootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_giRootSig)), "Create GI RootSig");

	ComPtr<IDxcBlob> cs = CompileDxc(kGatherShader.c_str(), L"CSMain", L"cs_6_5");

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _giRootSig.Get();
	pso.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
	ThrowIfFailed(_device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&_giPSO)), "Create GI PSO");
}

void D3D12Device::DispatchGI()
{
	// 프로브/depth 를 UAV 상태로 → RT 레이 수집 → 픽셀 읽기용 SRV 로 전환
	Transition(_probes.Get(), _probeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(_probeDepth.Get(), _probeDepthState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	_cmdList->SetPipelineState(_giPSO.Get());
	_cmdList->SetComputeRootSignature(_giRootSig.Get());
	_cmdList->SetComputeRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
	_cmdList->SetComputeRootUnorderedAccessView(1, _probes->GetGPUVirtualAddress());
	_cmdList->SetComputeRootUnorderedAccessView(2, _probeDepth->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(3, _tlas->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(4, _vb->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(5, _ib->GetGPUVirtualAddress());
	_cmdList->Dispatch((PROBE_COUNT + 63) / 64, 1, 1);

	D3D12_RESOURCE_BARRIER uav[2]{};
	uav[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav[0].UAV.pResource = _probes.Get();
	uav[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav[1].UAV.pResource = _probeDepth.Get();
	_cmdList->ResourceBarrier(2, uav);

	Transition(_probes.Get(), _probeState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Transition(_probeDepth.Get(), _probeDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void D3D12Device::CreateConstantBuffers()
{
	const size_t cbSize = (sizeof(SceneCB) + 255) & ~size_t(255); // 256 정렬
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		_cb[i] = CreateUploadBuffer(nullptr, cbSize);
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(_cb[i]->Map(0, &noRead, &_cbMapped[i]), "Map CB"); // 영속 매핑
	}
}


// 전체 플러시 — GPU 완료까지 대기 (스키닝이 VB 를 매 프레임 CPU 갱신하므로
// CPU/GPU 가 VB 를 동시에 만지지 않도록 프레임마다 직렬화. 단순/안전, 데모용)
void D3D12Device::WaitForGpu()
{
	const UINT64 v = ++_flushValue;
	ThrowIfFailed(_queue->Signal(_fence.Get(), v), "Queue Signal");
	if (_fence->GetCompletedValue() < v)
	{
		ThrowIfFailed(_fence->SetEventOnCompletion(v, _fenceEvent), "SetEventOnCompletion");
		WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void D3D12Device::MoveToNextFrame()
{
	WaitForGpu();
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::Destroy()
{
	if (_device)
		WaitForGpu();
	if (_editorReady)
	{
		_imgui.Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		_editorReady = false;
	}
	if (_fenceEvent)
	{
		CloseHandle(_fenceEvent);
		_fenceEvent = nullptr;
	}
}
