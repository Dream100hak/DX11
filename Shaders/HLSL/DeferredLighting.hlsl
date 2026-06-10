// DeferredLighting.hlsl
// Fullscreen deferred lighting pass — Cook-Torrance PBR (GGX + Smith + Schlick)
//
// G-Buffer SRV layout:
//   t0 : Albedo.rgb + Metallic (RGBA8)
//   t1 : Normal packed [0,1] + Roughness (RGBA16F)
//   t2 : World Position + mask (RGBA16F)
//   t3 : ShadowMap (forward 파이프라인과 공유)
//   t4 : SsaoMap (1 = 가림 없음)

#include "Common.hlsli"
#include "Shadow.hlsli"

// G-Buffer textures
Texture2D GBufferAlbedo   : register(t0);
Texture2D GBufferNormal   : register(t1);
Texture2D GBufferPosition : register(t2);
Texture2D ShadowMap       : register(t3);
Texture2D SsaoMap         : register(t4);

// IBL (Ibl::Init 에서 베이크)
TextureCube IrradianceMap  : register(t5);
TextureCube PrefilteredEnv : register(t6);
Texture2D   BrdfLut        : register(t7);

cbuffer IblBuffer : register(b8)
{
    int    UseIbl;
    float  EnvIntensity;
    float2 IblPad;
};

static const float PREFILTER_MAX_MIP = 4.0f; // Ibl::PREFILTER_MIPS - 1

// Multi-light data reused from Lighting.hlsli
struct LightData
{
    float4 diffuse;
    float4 ambient;
    float3 direction;
    float  intensity;
    float3 position;
    float  range;
    float3 attenuation;
    float  spotAngle;
    int    type;
    float3 pad;
};

cbuffer LightArrayBuffer : register(b7)
{
    LightData lights[16];
    int lightCount;
    float3 lightPadding;
};

struct LightingVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

// Fullscreen triangle from SV_VertexID (3 vertices, no vertex buffer)
LightingVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    LightingVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// ───────────────────────────────────────────────────────────
// Cook-Torrance BRDF
// ───────────────────────────────────────────────────────────
static const float PI = 3.14159265359f;

// GGX/Trowbridge-Reitz normal distribution
float DistributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-5f);
}

// Smith geometry (Schlick-GGX, direct lighting k)
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    float gV = NdotV / (NdotV * (1.0f - k) + k);
    float gL = NdotL / (NdotL * (1.0f - k) + k);
    return gV * gL;
}

// Schlick Fresnel
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// 러프니스 보정 Fresnel (IBL 앰비언트용)
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 fMax = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (fMax - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float4 PS_Main(LightingVSOutput input) : SV_TARGET
{
    float4 albedoData   = GBufferAlbedo.Sample(PointSampler, input.uv);
    float4 normalPacked = GBufferNormal.Sample(PointSampler, input.uv);
    float4 positionData = GBufferPosition.Sample(PointSampler, input.uv);

    if (positionData.w < 0.5f)
        discard;

    float3 albedo    = albedoData.rgb;
    float  metallic  = albedoData.a;
    float  roughness = max(normalPacked.w, 0.04f); // 0 러프니스 스펙큘러 폭주 방지
    float3 N         = normalize(normalPacked.xyz * 2.0f - 1.0f);
    float3 worldPos  = positionData.xyz;
    float3 V_        = normalize(CameraPositionWS() - worldPos);
    float  NdotV     = max(dot(N, V_), 1e-4f);

    // 비금속 기본 반사율 4%, 금속은 albedo 가 F0
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    // 섀도우 팩터 (디렉셔널 섀도우맵)
    float shadowFactor = 1.0f;
    float4 shadowHomoPos = mul(float4(worldPos, 1.0f), Shadow);
    float3 shadowNDC = shadowHomoPos.xyz / shadowHomoPos.w;
    if (shadowNDC.x > 0.01f && shadowNDC.x < 0.99f &&
        shadowNDC.y > 0.01f && shadowNDC.y < 0.99f &&
        shadowNDC.z > 0.0f  && shadowNDC.z < 1.0f)
    {
        shadowFactor = CalcShadowFactor(ShadowMap, shadowHomoPos);
    }

    float3 Lo = float3(0, 0, 0);           // 직접광 누적
    float3 ambientAccum = float3(0, 0, 0); // 앰비언트 누적

    [unroll(16)]
    for (int i = 0; i < lightCount; ++i)
    {
        float3 L = float3(0, 0, 1);
        float att = 1.0f;

        if (lights[i].type == 0)
        {
            L = -normalize(lights[i].direction);
        }
        else
        {
            float3 toLight = lights[i].position - worldPos;
            float d = length(toLight);
            if (d > lights[i].range) continue;
            L = toLight / d;
            att = 1.0f / (lights[i].attenuation.x + lights[i].attenuation.y * d + lights[i].attenuation.z * d * d);

            if (lights[i].type == 2)
            {
                float cosAngle = dot(-L, normalize(lights[i].direction));
                float outerCos = lights[i].spotAngle;
                float innerCos = lerp(outerCos, 1.0f, 0.3f);
                att *= saturate((cosAngle - outerCos) / (innerCos - outerCos));
            }
        }

        ambientAccum += lights[i].ambient.rgb * lights[i].intensity * att;

        float NdotL = dot(N, L);
        if (NdotL <= 0.0f)
            continue;

        float3 H = normalize(V_ + L);
        float NdotH = max(dot(N, H), 0.0f);
        float HdotV = max(dot(H, V_), 0.0f);

        float  D = DistributionGGX(NdotH, roughness);
        float  G = GeometrySmith(NdotV, NdotL, roughness);
        float3 F = FresnelSchlick(HdotV, F0);

        float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
        float3 kd = (1.0f - F) * (1.0f - metallic); // 금속은 디퓨즈 없음

        float3 radiance = lights[i].diffuse.rgb * lights[i].intensity * att;
        Lo += (kd * albedo / PI + specular) * radiance * NdotL;
    }

    // 라이트가 없으면 기본 환경광으로 실루엣만 보이게
    if (lightCount == 0)
        ambientAccum = float3(0.45f, 0.45f, 0.45f);

    // SSAO: 앰비언트 항에 가림도 적용 (UseSsao 플래그로 토글)
    float ssaoFactor = 1.0f;
    if (UseSsao)
        ssaoFactor = SsaoMap.SampleLevel(LinearSampler, input.uv, 0.0f).r;

    // 앰비언트: IBL (환경맵 디퓨즈 + 스펙큘러) 또는 라이트 ambient 폴백
    float3 ambient;
    if (UseIbl)
    {
        float3 F  = FresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kd = (1.0f - F) * (1.0f - metallic);

        float3 irradiance  = IrradianceMap.SampleLevel(LinearSampler, N, 0).rgb;
        float3 diffuseIBL  = kd * irradiance * albedo;

        float3 R = reflect(-V_, N);
        float3 prefiltered = PrefilteredEnv.SampleLevel(LinearSampler, R, roughness * PREFILTER_MAX_MIP).rgb;
        float2 brdf = BrdfLut.SampleLevel(LinearSampler, float2(NdotV, roughness), 0).rg;
        float3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y);

        ambient = (diffuseIBL + specularIBL) * EnvIntensity * ssaoFactor;
    }
    else
    {
        ambient = ambientAccum * albedo * ssaoFactor;
    }

    float3 litColor = ambient + shadowFactor * Lo;

    return float4(litColor, 1.0f);
}
