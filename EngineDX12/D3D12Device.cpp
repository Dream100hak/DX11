#include "D3D12Device.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "GeometryHelper.h"
#include "SkyRenderer.h"
#include "GridRenderer.h"
#include "SceneManager.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

D3D12Device* D3D12Device::s_main = nullptr;

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
    float4 gTint;       // rgb 디퓨즈 틴트, w 바닥 거칠기
    float4 gPtPos[16];  // 다중 점광원 위치+반경 (MAX_PT)
    float4 gPtCol[16];  // 다중 점광원 색×세기 (w on)
    float4 gFloorMat;   // rgb 바닥 albedo, w 바닥 metallic
    float4 gAO;         // x on, y intensity, z radius
    float4 gShade;      // x toonLevels(0=off), y rimPower(0=off), z normalIntensity, w checker(0/1)
    float4 gRimColor;   // rgb 림 색
    float4 gGridParams; // x cell, y fade, z bgMode(0 sky/1 solid), w _
    float4 gOutline;    // rgb 색, w thickness
    float4 gDecal;      // xyz 데칼 위치, w 반경(0=off)
    float4 gDecalCol;   // rgb 데칼 색, w 구름량(0=off)
    float4 gExtra;      // x shadowStrength, y hemiAmbient, z stars(0/1), w _
    float4 gFog2;       // x 높이안개 시작Y, y 낙폭, z on(0/1), w _ — 높이 기반 안개
    float4 gDecalArr[8];    // xyz 위치, w 반경(0=off) — 다중 데칼(상향 투영)
    float4 gDecalColArr[8]; // rgb 색, w on
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
cbuffer UseTexCB : register(b1) { uint gUseTex; float gObjMetal; float gObjRough; float gObjEmis; float3 gObjTint; float _b1pad; }; // per-object 머티리얼 루트상수 (0 바닥/1 텍스처/2 정점색)

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

    // ── 노멀 매핑 (TBN, 텍스처 모델만) — 탄젠트가 퇴화(0)면 지오메트릭 노멀 폴백(NaN 방지) ──
    if (gUseTex == 1)
    {
        float3 T = i.wtan - Ngeo * dot(i.wtan, Ngeo); // Gram-Schmidt
        if (dot(T, T) > 1e-5)
        {
            T = normalize(T);
            float3 B = cross(Ngeo, T);
            float3 nTS = gNormalMap.Sample(gSamp, i.uv).rgb * 2.0 - 1.0;
            nTS.xy *= gShade.z; // V9 노멀맵 강도
            N = normalize(nTS.x * T + nTS.y * B + nTS.z * Ngeo);
        }
    }

    float3 L = normalize(-gLightDir.xyz);
    float  ndl = saturate(dot(N, L));
    float3 V = normalize(gCamPos.xyz - i.wpos);

    // 머티리얼 (0=바닥 gFloorMat / 1=텍스처×틴트 / 2=정점색×틴트) — 1·2 는 per-object 루트상수
    float metallic  = (gUseTex == 0) ? gFloorMat.w : gObjMetal;
    float roughness = (gUseTex == 0) ? gTint.w     : gObjRough;
    float emissive  = (gUseTex == 0) ? 0.0          : gObjEmis;
    float3 albedo   = (gUseTex == 1) ? gDiffuse.Sample(gSamp, i.uv).rgb * gObjTint
                    : (gUseTex == 2) ? i.col * gObjTint   // per-object 정점색 × 틴트
                    : gFloorMat.rgb;
    // V16 체커 바닥
    if (gUseTex == 0 && gShade.w > 0.5)
    {
        float c = (fmod(floor(i.wpos.x) + floor(i.wpos.z), 2.0) < 1.0) ? 0.75 : 0.35;
        albedo = gFloorMat.rgb * c;
    }
    // W2 데칼 (바닥 투영, 단일 — 레거시)
    if (gUseTex == 0 && gDecal.w > 0.001)
    {
        float m = saturate(1.0 - distance(i.wpos.xz, gDecal.xz) / gDecal.w);
        albedo = lerp(albedo, gDecalCol.rgb, smoothstep(0.0, 1.0, m));
    }
    // 다중 데칼 (상향 XZ 투영) — 텍스처 없는 표면(바닥/터레인/프리미티브)에 적용
    if (gUseTex != 1)
    {
        [loop] for (int di = 0; di < 8; ++di)
        {
            if (gDecalColArr[di].w < 0.5 || gDecalArr[di].w < 0.001) continue;
            float m = saturate(1.0 - distance(i.wpos.xz, gDecalArr[di].xz) / gDecalArr[di].w);
            albedo = lerp(albedo, gDecalColArr[di].rgb, smoothstep(0.0, 1.0, m) * 0.95);
        }
    }
    float power = lerp(8.0, 256.0, 1.0 - roughness);
    float3 specColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float specMask = (gUseTex == 1) ? gSpecMap.Sample(gSamp, i.uv).r : 1.0;

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
    shadow = lerp(1.0 - gExtra.x, 1.0, shadow); // W6 그림자 강도(완전 검정 방지/조절)

    float3 sunCol = gSunColor.rgb * gLightDir.w;
    float df = ndl * shadow;
    if (gShade.x > 0.5) df = floor(df * gShade.x + 0.5) / gShade.x; // V2 툰(계단형 음영)
    float3 direct = albedo * df * sunCol * (1.0 - metallic * 0.75);
    float3 spec = 0;
    if (ndl > 0.0) { float3 H = normalize(L + V); spec += pow(saturate(dot(N, H)), power) * specMask * shadow * sunCol * specColor * (1.0 - roughness * 0.5); }

    // ── 다중 점광원 ──
    [loop] for (int p = 0; p < 16; ++p)
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

    // ── RT 앰비언트 오클루전 (반구 단거리 레이) ──
    float ao = 1.0;
    if (gAO.x > 0.5)
    {
        float occ = 0.0; const int AOK = 6;
        [unroll] for (int a = 0; a < AOK; ++a)
        {
            float ang = float(a) * 2.39963 + i.wpos.x * 7.1 + i.wpos.z * 3.3;
            float3 d = normalize(N + float3(cos(ang), abs(sin(ang * 1.3)), sin(ang)) * 0.75);
            RayDesc r; r.Origin = i.wpos + Ngeo * 0.02; r.Direction = d; r.TMin = 0.01; r.TMax = gAO.z;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> q;
            q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, r); q.Proceed();
            if (q.CommittedStatus() != COMMITTED_NOTHING) occ += 1.0;
        }
        ao = saturate(1.0 - (occ / AOK) * gAO.y);
    }
    if (dv == 4) return float4(indirect * ao, 1);

    float3 col = albedo * gGI.z * gSunColor.w * ao + direct + indirect * ao + spec;
    col += albedo * emissive;
    // W7 헤미스피어 앰비언트 (하늘=위 / 바닥=아래)
    if (gExtra.y > 0.001)
    {
        float3 skyA = lerp(gSkyHorizon.rgb, gSkyZenith.rgb, saturate(N.y));
        float3 hemi = lerp(gFloorMat.rgb * 0.3, skyA, N.y * 0.5 + 0.5) * gExtra.y;
        col += albedo * hemi * ao;
    }

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

    // ── V3 림 라이트 (프레넬 가장자리광) ──
    if (gShade.y > 0.5)
    {
        float rim = pow(1.0 - saturate(dot(N, V)), gShade.y);
        col += gRimColor.rgb * rim;
    }

    // ── 거리 안개 ──
    if (gFog.w > 1e-5)
    {
        float fogF = 1.0 - exp(-distance(gCamPos.xyz, i.wpos) * gFog.w);
        if (gFog2.z > 0.5) // 높이 안개 — 시작Y 위로 갈수록 옅어짐
            fogF *= saturate(exp(-max(0.0, i.wpos.y - gFog2.x) * gFog2.y));
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
)";

// 선택 아웃라인 — 인버티드 헐 (법선 방향 팽창, 앞면 컬링 → 가장자리 림)
static const std::string kOutlineShader = std::string(kSceneCB) + R"(
struct VIn { float3 pos:POSITION; float3 nrm:NORMAL; float3 col:COLOR; float2 uv:TEXCOORD; float3 tan:TANGENT; };
float4 VSMain(VIn i) : SV_POSITION
{
    float3 wp = i.pos;                 // 정점은 이미 월드(스키닝/기즈모 반영)
    float3 n = normalize(i.nrm);
    float d = distance(gCamPos.xyz, wp);
    wp += n * d * gOutline.w;          // 두께(카메라 거리 비례)
    return mul(float4(wp, 1.0), gMVP);
}
float4 PSMain() : SV_TARGET { return float4(gOutline.rgb, 1.0); } // 아웃라인 색(HDR)
)";

// GPU 인스턴스드 빌보드 파티클 — 입자당 1 인스턴스(6정점 쿼드), 카메라 정면 빌보드 + 가산 블렌드.
// CPU 시뮬(ParticleSystem) 결과를 인스턴스 버퍼로 올려 GPU 인스턴싱으로 렌더(디버그 크로스 대체).
static const std::string kParticleShader = std::string(kSceneCB) + R"(
struct Particle { float3 pos; float size; float3 col; float pad; }; // 32B (CPU PInst 와 동일)
StructuredBuffer<Particle> gParts : register(t1); // 루트 SRV(param2)로 바인드 — 입력레이아웃/VB 불필요
cbuffer BillCB : register(b1) { float3 gCamRight; float _p0; float3 gCamUp; float _p1; };
struct VOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float3 col:COLOR; };
VOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    float2 cs[6] = { float2(-1,-1), float2(1,-1), float2(1,1), float2(-1,-1), float2(1,1), float2(-1,1) };
    float2 c = cs[vid];
    Particle p = gParts[iid];
    float3 wp = p.pos + gCamRight * (c.x * p.size) + gCamUp * (c.y * p.size);
    VOut o; o.pos = mul(float4(wp, 1.0), gMVP); o.uv = c; o.col = p.col; return o;
}
float4 PSMain(VOut i) : SV_TARGET
{
    float r = saturate(1.0 - dot(i.uv, i.uv)); // 소프트 원형 폴오프
    return float4(i.col * r, r);               // 가산 블렌드 전제
}
)";

// 터레인 GPU 테셀레이션 — 코어스 쿼드 패치를 HS/DS 로 세분 + 하이트맵(StructuredBuffer) 변위.
// 메인 루트시그 재사용: b0(VP/빛), t1(하이트맵 float[]), b1(테셀 상수). PS=간단 람베르트(높이/경사 색).
static const std::string kTessShader = std::string(kSceneCB) + R"(
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
)";

// 물 평면 — 절차적 그리드(SV_VertexID) + 사인파 합성 변위 + 프레넬 하늘반사. 반투명 블렌드.
static const std::string kWaterShader = std::string(kSceneCB) + R"(
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
)";

// 디버그 라인 (본/AABB/스팟콘/라이트 아이콘) — LINELIST, pos+color
// 디버그 라인 셰이더는 DebugDraw.cpp 로 이동(자체 포함, kSceneCB 불필요)

// DDGI 프로브 시각화 — 프로브 위치마다 점(POINTLIST), 색 = DC irradiance
static const std::string kProbeViz = std::string(kSceneCB) + R"(
struct PSH { float3 c0; float3 c1; float3 c2; float3 c3; };
StructuredBuffer<PSH> gProbes : register(t1);
struct VOut { float4 pos:SV_POSITION; float3 col:COLOR; };
VOut VSMain(uint id : SV_VertexID)
{
    VOut o;
    uint PX = (uint)gGridDim.x, PY = (uint)gGridDim.y, PZ = (uint)gGridDim.z;
    uint px = id % PX, py = (id / PX) % PY, pz = id / (PX * PY);
    float3 t = float3(px / max(PX - 1.0, 1.0), py / max(PY - 1.0, 1.0), pz / max(PZ - 1.0, 1.0));
    float3 wp = lerp(gGridMin.xyz, gGridMax.xyz, t);
    o.pos = mul(float4(wp, 1.0), gMVP);
    o.col = max(gProbes[id].c0 * 0.886, 0.03);
    return o;
}
float4 PSMain(VOut i) : SV_TARGET { return float4(i.col, 1.0); }
)";


ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target); // 전방 선언


// ───────────────────────────────────────────────────────────
void D3D12Device::Init(HWND hwnd, UINT width, UINT height)
{
	s_main = this; // 전역 접근(Get) 등록
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
	_scene.Init(this);
	{
		wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
		_scene.Load(dir + L"\\..\\Resources\\Assets\\Models\\Archer\\Archer.mesh");
	}

	GET_SINGLE(TimeManager)->Init(); // 델타타임 기준점

	BuildGameScene(); // 씬 그래프(모델 GameObject + ModelRenderer) — 렌더 루프가 순회

	// Phase 3 — DDGI 프로브 볼륨
	CreateGI();

	// 포스트프로세스 (HDR 톤맵 / 블룸 / FXAA) — SceneRT SRV 생성 전에 힙/PSO 준비
	_postfx.Init(_device.Get(), _sceneFmt);
	_gamePostfx.Init(_device.Get(), _sceneFmt);                 // Game 뷰 전용 포스트
	_gameCB = CreateUploadBuffer(nullptr, sizeof(SceneCB));     // 게임 카메라 CB
	{ D3D12_RANGE nr{ 0,0 }; _gameCB->Map(0, &nr, &_gameCBMapped); }

	// 에디터 UI (ImGui DX12 + 도킹)
	InitEditor();
}

// 씬 그래프 구성 — 모델을 GameObject(Transform + ModelRenderer)로, SceneManager 현재 씬에 등록.
// 렌더 루프가 Scene 의 렌더러를 순회해 Draw(ctx) 한다 (DX11 Engine 의 Scene→Renderer 경로 이식 시작).
void D3D12Device::BuildGameScene()
{
	_gameScene = make_shared<Scene>();
	GET_SINGLE(SceneManager)->SetCurrentScene(_gameScene);

	_modelObj = make_shared<GameObject>();
	_modelObj->SetObjectName(L"Model");
	_modelObj->GetOrAddTransform();
	auto mr = make_shared<ModelRenderer>();
	mr->Bind(this);
	_modelObj->AddComponent(mr);
	_gameScene->Add(_modelObj);

	// 에디터 카메라 GameObject — Camera 컴포넌트 (GetMainCamera 캐시 대상). view/proj 는 FlyCamera 가 매 프레임 주입.
	_camObj = make_shared<GameObject>();
	_camObj->SetObjectName(L"EditorCamera");
	_camObj->SetEditorInternal(true);
	_camObj->GetOrAddTransform();
	_camObj->AddComponent(make_shared<Camera>());
	_gameScene->Add(_camObj);

	// 스카이박스(Background 큐) / 씬 그리드(Transparent 큐) GameObject
	{
		auto skyObj = make_shared<GameObject>();
		skyObj->SetObjectName(L"Sky"); skyObj->SetEditorInternal(true); skyObj->GetOrAddTransform();
		auto skyR = make_shared<SkyRenderer>(); skyR->Bind(this); skyObj->AddComponent(skyR);
		_gameScene->Add(skyObj);

		auto gridObj = make_shared<GameObject>();
		gridObj->SetObjectName(L"Grid"); gridObj->SetEditorInternal(true); gridObj->GetOrAddTransform();
		auto gridR = make_shared<GridRenderer>(); gridR->Bind(this); gridObj->AddComponent(gridR);
		_gameScene->Add(gridObj);
	}

	// 라이트 GameObject (씬 그래프 + CB 소스). 매 프레임 SyncLights 로 스칼라→컴포넌트 미러.
	auto addLight = [&](const wchar_t* name, LightType lt, Vec3 color, float intensity) -> shared_ptr<GameObject>
	{
		auto o = make_shared<GameObject>();
		o->SetObjectName(name); o->GetOrAddTransform();
		auto l = make_shared<Light>(); l->_lightType = lt; l->_color = color; l->_intensity = intensity;
		o->AddComponent(l);
		_gameScene->Add(o);
		return o;
	};
	_sunObj  = addLight(L"Directional Light", LightType::Directional, _sunColor, _lightIntensity);
	_ptObj   = addLight(L"Point Light",       LightType::Point,       _pointColor, _pointIntensity);
	_spotObj = addLight(L"Spot Light",        LightType::Spot,        _spotColor,  _spotIntensity);

	// 정적 메시 데모 — MeshRenderer 실드로우 검증용 큐브 (모델 옆, Opaque 버킷)
	{
		auto cubeObj = make_shared<GameObject>();
		cubeObj->SetObjectName(L"Cube");
		auto tr = cubeObj->GetOrAddTransform();
		tr->SetLocalPosition(Vec3{ 2.2f, 0.5f, 0.f });
		auto cmr = make_shared<MeshRenderer>(); cmr->Bind(this);
		vector<Vtx> cv; vector<uint32> ci; GeometryHelper::CreateCube(cv, ci, 1.0f, Vec3{ 1.f, 1.f, 1.f });
		cmr->SetGeometry(cv, ci); cmr->SetPrim(MeshPrim::Cube); // 복제/직렬화 복원 가능하도록 prim 지정
		// 절차적 체커 텍스처 (SRV 바인딩 경로 검증, 파일 의존 없음)
		{
			const uint32 N = 64; std::vector<uint8_t> tex(N * N * 4);
			for (uint32 y = 0; y < N; ++y) for (uint32 x = 0; x < N; ++x)
			{
				bool c = ((x / 8) + (y / 8)) & 1;
				uint8_t* p = &tex[(y * N + x) * 4];
				p[0] = c ? 230 : 60; p[1] = c ? 120 : 90; p[2] = c ? 60 : 200; p[3] = 255;
			}
			cmr->SetTexturePixels(tex, N, N);
		}
		cubeObj->AddComponent(cmr);
		_gameScene->Add(cubeObj);
	}
}

// 스칼라 라이팅 파라미터 → Light 컴포넌트 미러 (CB 가 컴포넌트에서 읽도록). 무중단 전환.
void D3D12Device::SyncLights()
{
	if (_sunObj) { auto l = _sunObj->GetLight(); l->_color = _sunColor; l->_intensity = _lightIntensity;
		float a = _lightAngle; l->_direction = Vec3{ cosf(a) * 0.6f, -1.f, sinf(a) * 0.6f }; }
	if (_ptObj)  { auto l = _ptObj->GetLight(); l->_enabled = _pointOn; l->_color = _pointColor; l->_intensity = _pointIntensity; l->_range = _pointRadius;
		if (auto t = _ptObj->GetTransform()) t->SetLocalPosition(_pointPos); }
	if (_spotObj){ auto l = _spotObj->GetLight(); l->_enabled = _spotOn; l->_color = _spotColor; l->_intensity = _spotIntensity; l->_range = _spotRadius;
		l->_spotAngleDeg = _spotConeDeg; l->_direction = _spotDir;
		if (auto t = _spotObj->GetTransform()) t->SetLocalPosition(_spotPos); }
}

void D3D12Device::EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		debug->EnableDebugLayer();
	// DRED — device removed 시 어떤 GPU 명령에서 터졌는지 breadcrumb/page-fault 기록
	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred))))
	{
		dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	}
#endif
}

// device removed 시 DRED breadcrumb/page-fault 덤프 (어느 GPU op 인지 추적)
void D3D12Device::DumpDeviceRemoved()
{
	HRESULT reason = _device ? _device->GetDeviceRemovedReason() : 0;
	char hdr[128]; sprintf_s(hdr, "\n===== DEVICE REMOVED  reason=0x%08X =====\n", (unsigned)reason);
	OutputDebugStringA(hdr); Log(hdr);
#if defined(_DEBUG)
	ComPtr<ID3D12DeviceRemovedExtendedData> dred;
	if (_device && SUCCEEDED(_device->QueryInterface(IID_PPV_ARGS(&dred))))
	{
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT bc{};
		if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&bc)))
		{
			const D3D12_AUTO_BREADCRUMB_NODE* n = bc.pHeadAutoBreadcrumbNode;
			while (n)
			{
				UINT done = n->pLastBreadcrumbValue ? *n->pLastBreadcrumbValue : 0;
				if (done < n->BreadcrumbCount) // 미완료 노드 = 폴트 지점 근처
				{
					char b[256]; sprintf_s(b, "[DRED] %S / %S  op %u/%u 에서 중단\n",
						n->pCommandQueueDebugNameW ? n->pCommandQueueDebugNameW : L"?",
						n->pCommandListDebugNameW ? n->pCommandListDebugNameW : L"?", done, n->BreadcrumbCount);
					OutputDebugStringA(b); Log(b);
					for (UINT i = done; i < n->BreadcrumbCount && i < done + 4; ++i)
					{ char o[64]; sprintf_s(o, "   op[%u]=%d\n", i, (int)n->pCommandHistory[i]); OutputDebugStringA(o); }
				}
				n = n->pNext;
			}
		}
		D3D12_DRED_PAGE_FAULT_OUTPUT pf{};
		if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pf)))
		{ char b[128]; sprintf_s(b, "[DRED] PageFault VA=0x%llX\n", (unsigned long long)pf.PageFaultVA); OutputDebugStringA(b); Log(b); }
	}
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

// 창 크기 변경 — GPU 유휴 후 백버퍼 해제 → ResizeBuffers → RTV 재생성.
// 씬/게임 RT 는 ImGui 영역 기준으로 매 프레임 별도 리사이즈되므로 백버퍼만 처리.
void D3D12Device::OnResize(UINT width, UINT height)
{
	if (!_swapChain || width == 0 || height == 0) return;        // 최소화/초기화 전 무시
	if (width == _width && height == _height) return;

	WaitForGpu(); // 모든 프레임 in-flight 명령 완료 대기

	for (UINT i = 0; i < FRAME_COUNT; ++i) _renderTargets[i].Reset(); // 기존 백버퍼 참조 해제(ResizeBuffers 전제)

	_width = width; _height = height;

	DXGI_SWAP_CHAIN_DESC1 d{}; _swapChain->GetDesc1(&d);
	ThrowIfFailed(_swapChain->ResizeBuffers(FRAME_COUNT, width, height, d.Format, d.Flags), "ResizeBuffers");
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();

	// 백버퍼 RTV 재생성 (기존 _rtvHeap 슬롯 재사용)
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargets[i])), "GetBuffer resize");
		_device->CreateRenderTargetView(_renderTargets[i].Get(), nullptr, rtv);
		rtv.ptr += _rtvDescSize;
	}

	// 프레임별 펜스값을 현재값으로 정렬 (MoveToNextFrame 일관성)
	for (UINT i = 0; i < FRAME_COUNT; ++i) _fenceValues[i] = _fenceValues[_frameIndex];
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
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 프로브 시각화 VS 에서도 읽음

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &texRange;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[4].Constants.ShaderRegister = 1; // b1
	params[4].Constants.Num32BitValues = 8; // mode + metallic/roughness/emissive + tint.rgb + pad (per-object) / 파티클 빌보드 기저
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 파티클 VS 도 b1(빌보드 기저) 읽음

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

	// ── 파티클 빌보드 PSO (StructuredBuffer 인스턴싱, 가산 블렌드, 깊이 테스트/쓰기 없음) ──
	{
		ComPtr<IDxcBlob> pvs = CompileDxc(kParticleShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> pps = CompileDxc(kParticleShader.c_str(), L"PSMain", L"ps_6_5");
		// 검증된 그리드 PSO(gpso)를 베이스로 — 입력레이아웃 없음/TRIANGLE/깊이테스트/DSV 동일, 차이만 오버라이드
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pp = gpso;
		pp.VS = { pvs->GetBufferPointer(), pvs->GetBufferSize() };
		pp.PS = { pps->GetBufferPointer(), pps->GetBufferSize() };
		pp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pp.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;      // 가산 블렌드
		pp.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		pp.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&pp, IID_PPV_ARGS(&_particlePSO)), "CreateParticlePSO");
	}

	// ── 터레인 테셀레이션 PSO (VS/HS/DS/PS, PATCH 토폴로지) — 메인 opaque pso 베이스 ──
	{
		ComPtr<IDxcBlob> tvs = CompileDxc(kTessShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> ths = CompileDxc(kTessShader.c_str(), L"HSMain", L"hs_6_5");
		ComPtr<IDxcBlob> tds = CompileDxc(kTessShader.c_str(), L"DSMain", L"ds_6_5");
		ComPtr<IDxcBlob> tps = CompileDxc(kTessShader.c_str(), L"PSMain", L"ps_6_5");
		D3D12_INPUT_ELEMENT_DESC til[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC tp = pso; // 메인 opaque(깊이 쓰기/테스트, RTV/DSV) 베이스
		tp.VS = { tvs->GetBufferPointer(), tvs->GetBufferSize() };
		tp.HS = { ths->GetBufferPointer(), ths->GetBufferSize() };
		tp.DS = { tds->GetBufferPointer(), tds->GetBufferSize() };
		tp.PS = { tps->GetBufferPointer(), tps->GetBufferSize() };
		tp.InputLayout = { til, _countof(til) };
		tp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		tp.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; // 테셀레이션
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&tp, IID_PPV_ARGS(&_tessPSO)), "CreateTessPSO");
		tp.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // 와이어 변형(테셀 밀도 시각화)
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&tp, IID_PPV_ARGS(&_tessWirePSO)), "CreateTessWirePSO");
	}

	// ── 물 평면 PSO (절차 그리드, 알파 블렌드, 깊이 테스트/쓰기) — 그리드 PSO(gpso) 베이스 ──
	{
		ComPtr<IDxcBlob> wvs = CompileDxc(kWaterShader.c_str(), L"VSMain", L"vs_6_5");
		ComPtr<IDxcBlob> wps = CompileDxc(kWaterShader.c_str(), L"PSMain", L"ps_6_5");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC wp = gpso; // 입력레이아웃 없음/TRIANGLE/알파블렌드 베이스
		wp.VS = { wvs->GetBufferPointer(), wvs->GetBufferSize() };
		wp.PS = { wps->GetBufferPointer(), wps->GetBufferSize() };
		wp.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		wp.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 물 표면 깊이 기록(파티클/그리드가 가려지게)
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&wp, IID_PPV_ARGS(&_waterPSO)), "CreateWaterPSO");
	}

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

	// ── 프로브 시각화 PSO (POINTLIST, 깊이 테스트) ──
	ComPtr<IDxcBlob> pvs = CompileDxc(kProbeViz.c_str(), L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> pps = CompileDxc(kProbeViz.c_str(), L"PSMain", L"ps_6_5");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ppso{};
	ppso.pRootSignature = _rootSig.Get();
	ppso.VS = { pvs->GetBufferPointer(), pvs->GetBufferSize() };
	ppso.PS = { pps->GetBufferPointer(), pps->GetBufferSize() };
	ppso.RasterizerState = rast;
	ppso.BlendState = blend;
	ppso.DepthStencilState.DepthEnable = TRUE; ppso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ppso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ppso.SampleMask = UINT_MAX;
	ppso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	ppso.NumRenderTargets = 1; ppso.RTVFormats[0] = _sceneFmt;
	ppso.DSVFormat = DXGI_FORMAT_D32_FLOAT; ppso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&ppso, IID_PPV_ARGS(&_probePSO)), "CreateProbePSO");

	// ── 디버그 라인(본/AABB/콘/아이콘/파티클) — DebugDraw 클래스가 PSO 2종 생성 ──
	_debugDraw.Init(_device.Get(), _rootSig.Get(), _sceneFmt);
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
	// gather 컴퓨트 셰이더는 공용 SceneCB 레이아웃에 의존 → 여기서 컴파일 후 바이트코드만 Ddgi 에 전달
	ComPtr<IDxcBlob> cs = CompileDxc(kGatherShader.c_str(), L"CSMain", L"cs_6_5");
	_ddgi.Create(_device.Get(), cs->GetBufferPointer(), cs->GetBufferSize());
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
