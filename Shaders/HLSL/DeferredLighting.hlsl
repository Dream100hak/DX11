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
Texture2D      GBufferAlbedo   : register(t0);
Texture2D      GBufferNormal   : register(t1);
Texture2D      GBufferPosition : register(t2);
Texture2DArray ShadowMap       : register(t3); // CSM 캐스케이드 배열
Texture2D      SsaoMap         : register(t4);

// IBL (Ibl::Init 에서 베이크)
TextureCube IrradianceMap  : register(t5);
TextureCube PrefilteredEnv : register(t6);
Texture2D   BrdfLut        : register(t7);

// Emissive RT (GBuffer SV_Target3 — linear HDR, 가산되어 블룸으로 흘러감)
Texture2D GBufferEmissive  : register(t8);

cbuffer IblBuffer : register(b8)
{
    int    UseIbl;
    float  EnvIntensity;
    float2 IblPad;
};

// CSM (C++ CascadeDesc 와 일치, CASCADE_COUNT=4)
cbuffer CascadeBuffer : register(b9)
{
    matrix CascadeVPT[4];   // 캐스케이드별 V*P*T
    float4 CascadeSplits;   // 각 캐스케이드 far 의 카메라 뷰공간 거리
    int    CascadeCount;
    int    CascadeDebug;    // 1 = 캐스케이드 색 틴트
    float2 CascadePad;
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
    int    shadowIndex;   // 점/스팟 그림자 슬롯 (-1 = 없음)
    float2 pad;
};

// ── Clustered shading (b7 + t11~t13) — Engine/ClusterLighting.cpp 와 레이아웃 일치 ──
cbuffer ClusterParams : register(b7)
{
    uint3  ClusterGrid;         // (16,9,24)
    uint   MaxLightsPerCluster; // 64
    float  ClusterZNear;
    float  ClusterZFar;
    uint   PunctualCount;       // ClusterLights 유효 개수
    uint   DirCount;            // DirLights 유효 개수
    float2 ClusterScreen;
    float2 ClusterPad;
    LightData DirLights[4];     // 디렉셔널(화면 전체) — 클러스터 컬링 대상 아님
};

StructuredBuffer<LightData> ClusterLights  : register(t11); // 펑추얼(점/스팟) 라이트
StructuredBuffer<uint>      ClusterCounts  : register(t12); // 클러스터별 라이트 수
StructuredBuffer<uint>      ClusterIndices : register(t13); // 평탄 라이트 인덱스 리스트

// 점/스팟 그림자
Texture2DArray   SpotShadowMap  : register(t9);  // 스팟 원근 섀도우 배열
TextureCubeArray PointShadowCube : register(t10); // 포인트 큐브 섀도우 배열 (깊이)
cbuffer PunctualShadowBuffer : register(b10)
{
    matrix SpotVPT[4];
};

// 포인트 큐브 그림자 — 방향으로 샘플, 메이저축 거리로 NDC 깊이 재구성해 비교
//  lightPos→frag 방향의 큐브 깊이(가장 가까운 캐스터)와 현재 프래그 깊이 비교
float PointShadowFactor(int cubeIndex, float3 fragToLight, float range)
{
    float3 dir = -fragToLight; // light → frag
    float  major = max(abs(dir.x), max(abs(dir.y), abs(dir.z)));
    float  nearZ = 0.1f;
    float  farZ = max(range, nearZ + 1.0f);
    // XMMatrixPerspectiveFovLH 의 z_ndc = f*(viewZ-n)/(viewZ*(f-n)), viewZ = major
    float  compareDepth = farZ * (major - nearZ) / (major * (farZ - nearZ));
    compareDepth -= 0.003f; // 바이어스 (셀프 섀도우 방지)
    return PointShadowCube.SampleCmpLevelZero(ShadowSampler, float4(dir, (float)cubeIndex), compareDepth).r;
}

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

// 단일 라이트 Cook-Torrance 기여 (radiance/그림자 제외, NdotL 포함)
float3 ShadePBR(float3 N, float3 V_, float3 L, float NdotV,
                float3 albedo, float metallic, float roughness, float3 F0)
{
    float  NdotL = saturate(dot(N, L));
    float3 H     = normalize(V_ + L);
    float  NdotH = max(dot(N, H), 0.0f);
    float  HdotV = max(dot(H, V_), 0.0f);

    float  D = DistributionGGX(NdotH, roughness);
    float  G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, F0);

    float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
    float3 kd = (1.0f - F) * (1.0f - metallic); // 금속은 디퓨즈 없음
    return (kd * albedo / PI + specular) * NdotL;
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

    // 섀도우 팩터 (CSM) — 카메라 뷰공간 깊이로 캐스케이드 선택 후 해당 슬라이스 PCF
    float viewZ = mul(float4(worldPos, 1.0f), V).z;
    int cascade = CascadeCount - 1;
    [unroll]
    for (int ci = 0; ci < CascadeCount; ++ci)
    {
        if (viewZ <= CascadeSplits[ci]) { cascade = ci; break; }
    }

    float shadowFactor = 1.0f;
    float4 shadowHomoPos = mul(float4(worldPos, 1.0f), CascadeVPT[cascade]);
    float3 shadowNDC = shadowHomoPos.xyz / shadowHomoPos.w;
    if (shadowNDC.x > 0.001f && shadowNDC.x < 0.999f &&
        shadowNDC.y > 0.001f && shadowNDC.y < 0.999f &&
        shadowNDC.z > 0.0f   && shadowNDC.z < 1.0f)
    {
        shadowFactor = CalcShadowFactorArray(ShadowMap, cascade, shadowHomoPos);
    }

    float3 Lo = float3(0, 0, 0);           // 직접광 누적
    float3 ambientAccum = float3(0, 0, 0); // 앰비언트 누적

    // ── 디렉셔널 라이트 (화면 전체) — CSM 그림자 ──
    [loop]
    for (uint d = 0; d < DirCount; ++d)
    {
        ambientAccum += DirLights[d].ambient.rgb * DirLights[d].intensity;

        float3 L = -normalize(DirLights[d].direction);
        if (dot(N, L) <= 0.0f) continue;

        float3 radiance = DirLights[d].diffuse.rgb * DirLights[d].intensity;
        Lo += shadowFactor * ShadePBR(N, V_, L, NdotV, albedo, metallic, roughness, F0) * radiance;
    }

    // ── 펑추얼 라이트 (점/스팟) — 픽셀이 속한 클러스터의 라이트만 순회 ──
    {
        uint cx = min((uint)(input.uv.x * ClusterGrid.x), ClusterGrid.x - 1);
        uint cy = min((uint)(input.uv.y * ClusterGrid.y), ClusterGrid.y - 1);
        // Z 로그 슬라이스 (ClusterLighting.cpp 의 sliceFromZ 와 동일 매핑)
        float sliceF = log(max(viewZ, ClusterZNear) / ClusterZNear) / log(ClusterZFar / ClusterZNear) * ClusterGrid.z;
        uint cz = (uint)clamp(sliceF, 0.0f, (float)ClusterGrid.z - 1.0f);
        uint clusterIdx = cx + cy * ClusterGrid.x + cz * ClusterGrid.x * ClusterGrid.y;

        uint count = ClusterCounts[clusterIdx];
        uint baseI = clusterIdx * MaxLightsPerCluster;

        [loop]
        for (uint li = 0; li < count; ++li)
        {
            LightData lt = ClusterLights[ClusterIndices[baseI + li]];

            float3 toLight = lt.position - worldPos;
            float dist = length(toLight);
            if (dist > lt.range) continue;
            float3 L = toLight / dist;

            // (1-t²)² 거리 윈도우 (t = d/range)
            float distT = dist / lt.range;
            float window = saturate(1.0f - distT * distT);
            float att = window * window;

            if (lt.type == 2) // 스팟 콘
            {
                float cosAngle = dot(-L, normalize(lt.direction));
                float outerCos = lt.spotAngle;
                float innerCos = lerp(outerCos, 1.0f, 0.3f);
                att *= saturate((cosAngle - outerCos) / (innerCos - outerCos));
            }

            ambientAccum += lt.ambient.rgb * lt.intensity * att;

            float NdotL = dot(N, L);
            if (NdotL <= 0.0f) continue;

            // 라이트별 그림자: 포인트=큐브, 스팟=원근 섀도우맵 슬롯
            float lightShadow = 1.0f;
            if (lt.type == 1 && lt.shadowIndex >= 0)
            {
                lightShadow = PointShadowFactor(lt.shadowIndex, lt.position - worldPos, lt.range);
            }
            else if (lt.type == 2 && lt.shadowIndex >= 0)
            {
                float4 sp = mul(float4(worldPos, 1.0f), SpotVPT[lt.shadowIndex]);
                if (sp.w > 0.0f)
                {
                    float3 sndc = sp.xyz / sp.w;
                    if (sndc.x > 0.001f && sndc.x < 0.999f &&
                        sndc.y > 0.001f && sndc.y < 0.999f &&
                        sndc.z > 0.0f   && sndc.z < 1.0f)
                    {
                        float spotBias = 0.0015f + 0.006f * (1.0f - saturate(NdotL));
                        lightShadow = CalcShadowFactorArray(SpotShadowMap, lt.shadowIndex, sp, spotBias);
                    }
                }
            }

            float3 radiance = lt.diffuse.rgb * lt.intensity * att;
            Lo += lightShadow * ShadePBR(N, V_, L, NdotV, albedo, metallic, roughness, F0) * radiance;
        }
    }

    // 라이트가 없으면 기본 환경광으로 실루엣만 보이게
    if (DirCount == 0 && PunctualCount == 0)
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

    // 발광은 라이팅/그림자와 무관하게 가산 — HDR 값이면 BrightPass 를 넘겨 블룸 글로우
    float3 emissive = GBufferEmissive.Sample(PointSampler, input.uv).rgb;

    float3 litColor = ambient + Lo + emissive; // 그림자는 라이트별로 Lo 에 이미 적용됨

    // 캐스케이드 디버그 틴트
    if (CascadeDebug)
    {
        const float3 tints[4] =
        {
            float3(1.0f, 0.55f, 0.55f), float3(0.55f, 1.0f, 0.55f),
            float3(0.55f, 0.7f, 1.0f),  float3(1.0f, 1.0f, 0.55f)
        };
        litColor *= tints[cascade];
    }

    return float4(litColor, 1.0f);
}
