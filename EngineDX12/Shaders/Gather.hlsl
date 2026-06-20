// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

#define OCT 8        // 프로브당 옥타헤드럴 depth 해상도 (8×8=64 텍셀)
#define RAY_CAP 128  // hitDist 로컬 배열 상한 (= gGridDim.w)
struct Vtx { float3 pos; float3 nrm; float3 col; float2 uv; float3 tan; }; // C++ Vtx 와 동일 레이아웃
struct ProbeSH { float3 c0; float3 c1; float3 c2; float3 c3; };

RaytracingAccelerationStructure gScene   : register(t0);
StructuredBuffer<Vtx>           gVerts    : register(t1);
StructuredBuffer<uint>          gIndices  : register(t2);
StructuredBuffer<uint2>         gInstMeta : register(t3); // 인스턴스별 {vbBase, ibBase} (집계 지오메트리 페치)
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
    uint pi = (uint)gGI.w + tid.x; // gGI.w = 프로브 베이스 오프셋 (라운드로빈 부분 갱신)
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
            uint2 meta = gInstMeta[q.CommittedInstanceID()];      // {vbBase, ibBase}
            uint i0 = gIndices[meta.y + prim * 3 + 0], i1 = gIndices[meta.y + prim * 3 + 1], i2 = gIndices[meta.y + prim * 3 + 2];
            Vtx v0 = gVerts[meta.x + i0], v1 = gVerts[meta.x + i1], v2 = gVerts[meta.x + i2];
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
