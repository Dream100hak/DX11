// IblBake.hlsl
// IBL 프리컴퓨트 — 시작 시 1회 베이크 (Ibl::Init)
//   PS_Irradiance : 코사인 가중 반구 컨볼루션 → 32x32 큐브 (디퓨즈 IBL)
//   PS_Prefilter  : GGX 중요도 샘플링 → 128x128 큐브 mip 체인 (스펙큘러 IBL, mip=roughness)
//   PS_BrdfLut    : Karis split-sum BRDF 적분 → 512x512 2D LUT (x=NdotV, y=roughness)
//
// t0 : 환경 큐브맵 (LDR sRGB — 샘플 시 linear 변환)
// b8 : IblBakeBuffer

#include "Common.hlsli"

TextureCube EnvMap : register(t0);

cbuffer IblBakeBuffer : register(b8)
{
    int    FaceIndex;   // 0..5 (+X -X +Y -Y +Z -Z)
    float  BakeRoughness;
    float2 BakePad;
};

static const float PI = 3.14159265359f;

struct BakeVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

BakeVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    BakeVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// 큐브맵 face + uv → 월드 방향 (D3D11 face 순서/방향)
float3 UvToDir(float2 uv, int face)
{
    float2 st = uv * 2.0f - 1.0f;
    float x = st.x;
    float y = -st.y;
    if (face == 0) return normalize(float3( 1.0f,  y, -x)); // +X
    if (face == 1) return normalize(float3(-1.0f,  y,  x)); // -X
    if (face == 2) return normalize(float3( x,  1.0f, -y)); // +Y
    if (face == 3) return normalize(float3( x, -1.0f,  y)); // -Y
    if (face == 4) return normalize(float3( x,  y,  1.0f)); // +Z
    return            normalize(float3(-x,  y, -1.0f));     // -Z
}

// 환경맵 샘플 (sRGB → linear)
float3 SampleEnv(float3 dir)
{
    float3 c = EnvMap.SampleLevel(LinearSampler, dir, 0).rgb;
    return pow(abs(c), 2.2f);
}

// ── 디퓨즈 irradiance: 반구 코사인 컨볼루션 ────────────────────────────────
float4 PS_Irradiance(BakeVSOutput input) : SV_TARGET
{
    float3 N = UvToDir(input.uv, FaceIndex);
    float3 up = abs(N.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = float3(0, 0, 0);
    float sampleCount = 0.0f;

    const float dPhi = 0.05f;
    const float dTheta = 0.05f;
    for (float phi = 0.0f; phi < 2.0f * PI; phi += dPhi)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += dTheta)
        {
            float3 tangentDir = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleDir = tangentDir.x * right + tangentDir.y * up + tangentDir.z * N;
            irradiance += SampleEnv(sampleDir) * cos(theta) * sin(theta);
            sampleCount += 1.0f;
        }
    }
    irradiance = PI * irradiance / sampleCount;
    return float4(irradiance, 1.0f);
}

// ── Hammersley + GGX 중요도 샘플링 ─────────────────────────────────────────
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0f * PI * xi.x;
    float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// ── 스펙큘러 prefilter: roughness 별 GGX 컨볼루션 (mip 체인) ────────────────
float4 PS_Prefilter(BakeVSOutput input) : SV_TARGET
{
    float3 N = UvToDir(input.uv, FaceIndex);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 256u;
    float3 prefiltered = float3(0, 0, 0);
    float totalWeight = 0.0f;

    [loop]
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(xi, N, BakeRoughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = dot(N, L);
        if (NdotL > 0.0f)
        {
            prefiltered += SampleEnv(L) * NdotL;
            totalWeight += NdotL;
        }
    }
    prefiltered /= max(totalWeight, 0.001f);
    return float4(prefiltered, 1.0f);
}

// ── BRDF 적분 LUT (Karis split-sum) ────────────────────────────────────────
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0f; // IBL 용 k
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX_IBL(NdotV, roughness) * GeometrySchlickGGX_IBL(NdotL, roughness);
}

float4 PS_BrdfLut(BakeVSOutput input) : SV_TARGET
{
    float NdotV = max(input.uv.x, 0.001f);
    float roughness = input.uv.y;

    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float3 N = float3(0, 0, 1);

    float A = 0.0f;
    float B = 0.0f;

    const uint SAMPLE_COUNT = 512u;
    [loop]
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G = GeometrySmith_IBL(NdotV, NdotL, roughness);
            float GVis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0f - VdotH, 5.0f);
            A += (1.0f - Fc) * GVis;
            B += Fc * GVis;
        }
    }
    return float4(A / SAMPLE_COUNT, B / SAMPLE_COUNT, 0.0f, 1.0f);
}
