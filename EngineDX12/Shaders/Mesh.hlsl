// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

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
    // ── IBL: 큐브맵 베이크 SH 환경 이라디언스 (이미지 기반 앰비언트) ──
    // 디퓨즈 = albedo × irradiance / π (정규화). gEnvSH[0].w = 강도.
    if (gEnvSH[0].w > 0.0) col += albedo * EvalEnvIrradiance(N) * (gEnvSH[0].w * 0.318310) * ao;
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
        else { float t = saturate(R.y); refl = (gEnvSH[0].w > 0.0) ? EvalEnvIrradiance(R) * gEnvSH[0].w : lerp(gSkyHorizon.rgb, gSkyZenith.rgb, pow(t, 0.55)); }
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
