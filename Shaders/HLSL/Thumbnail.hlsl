// Thumbnail.hlsl
// 메시 프리뷰 썸네일용 - 단순 Lambert + Ambient

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

// ── PS (단색 / 텍스처 없음) ───────────────────────────────────
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

// ── PS (와이어프레임 오버레이) ────────────────────────────────
float4 PS_Wireframe(MeshOutput input) : SV_TARGET
{
    float3 lightDir   = normalize(float3(0.f, -1.f, 0.f));
    float3 lightColor = float3(0.3f, 0.3f, 0.3f);
    float3 normal = normalize(input.normal);

    float ndotl = max(dot(normal, -lightDir), 0.f);
    float3 diffuse  = ndotl * lightColor;
    float3 ambient= float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambient + diffuse;

    // 와이어프레임 색과 50% 혼합
    float3 wireColor = float3(1.f, 1.f, 1.f);
 finalColor = lerp(finalColor, wireColor, 0.5f);

    return float4(finalColor, 1.f);
}
