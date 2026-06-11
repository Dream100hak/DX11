// Thumbnail.hlsl
// 메시 프리뷰/썸네일용 PS 모음 (그림자/SSAO/라이트배열 의존 없음, 상태 누수 없음)

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

static const float PREVIEW_PI = 3.14159265359f;

// ── Cook-Torrance 헬퍼 (DeferredLighting.hlsl 과 동일 모델) ────────────────
float PreviewDistributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PREVIEW_PI * d * d, 1e-5f);
}

float PreviewGeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    float gV = NdotV / (NdotV * (1.0f - k) + k);
    float gL = NdotL / (NdotL * (1.0f - k) + k);
    return gV * gL;
}

float3 PreviewFresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// 모델 프리뷰/썸네일 — Cook-Torrance PBR (씬 디퍼드 라이팅과 동일 모델, 고정 라이트 1개)
// 프리뷰 RT 는 톤매핑 패스를 거치지 않으므로 여기서 Reinhard + 감마 인코딩
float4 PS_PreviewLit(MeshOutput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 V = normalize(CameraPositionWS() - input.worldPosition);
    float3 L = -normalize(float3(0.3f, -1.0f, -0.4f));

    float4 tex = MatDiffuse;
    if (UseTexture)
        tex = DiffuseMap.Sample(LinearSampler, input.uv) * MatDiffuse; // 틴트 곱 (GBuffer 와 동일 규약)
    float3 albedo = pow(abs(tex.rgb), 2.2f); // sRGB -> linear

    float roughness = max(Roughness, 0.04f);
    float metallic  = Metallic;
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 1e-4f);

    float3 Lo = float3(0, 0, 0);
    if (NdotL > 0.0f)
    {
        float3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0f);
        float HdotV = max(dot(H, V), 0.0f);

        float  D = PreviewDistributionGGX(NdotH, roughness);
        float  G = PreviewGeometrySmith(NdotV, NdotL, roughness);
        float3 F = PreviewFresnelSchlick(HdotV, F0);

        float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
        float3 kd = (1.0f - F) * (1.0f - metallic);

        float3 radiance = float3(1.6f, 1.6f, 1.6f);
        Lo = (kd * albedo / PREVIEW_PI + specular) * radiance * NdotL;
    }

    // 상수 환경 근사 — 디퓨즈 + 금속용 스펙큘러 (IBL 없는 프리뷰에서 금속이 검지 않도록)
    float3 ambient = 0.35f * albedo * (1.0f - metallic) + 0.25f * F0;

    float3 color = ambient + Lo;
    color = color / (color + 1.0f);          // Reinhard
    color = pow(abs(color), 1.0f / 2.2f);    // 감마
    return float4(color, 1.0f);
}

// ── 단색 PS (텍스처 없음) ──────────────────────────────────────────────────
float4 PS_Solid(MeshOutput input) : SV_TARGET
{
    float3 lightDir   = normalize(float3(0.f, -1.f, 0.f));
    float3 lightColor = float3(0.3f, 0.3f, 0.3f);
    float3 normal     = normalize(input.normal);

    float ndotl = max(dot(normal, -lightDir), 0.f);
    float3 diffuse  = ndotl * lightColor;
    float3 ambient  = float3(0.5f, 0.5f, 0.5f);

    return float4(ambient + diffuse, 1.f);
}

// ── 와이어프레임 오버레이 PS ──────────────────────────────────────────────
float4 PS_Wireframe(MeshOutput input) : SV_TARGET
{
    float3 lightDir   = normalize(float3(0.f, -1.f, 0.f));
    float3 lightColor = float3(0.3f, 0.3f, 0.3f);
    float3 normal = normalize(input.normal);

    float ndotl = max(dot(normal, -lightDir), 0.f);
    float3 diffuse  = ndotl * lightColor;
    float3 ambient  = float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambient + diffuse;

    // 와이어 컬러 50% 혼합
    float3 wireColor = float3(1.f, 1.f, 1.f);
    finalColor = lerp(finalColor, wireColor, 0.5f);

    return float4(finalColor, 1.f);
}
