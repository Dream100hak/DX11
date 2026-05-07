// Lighting.hlsli
// Phong/Blinn-Phong 라이팅 함수
// Common.hlsli 의 LightBuffer / MaterialBuffer 에 의존

#ifndef _LIGHTING_HLSLI_
#define _LIGHTING_HLSLI_

#include "Common.hlsli"

// ===========================================================
// Directional Light  (Blinn-Phong)
// ===========================================================
void ComputeDirectionalLight(
    float3 normal,
    float3 toEye,
    out float4 ambient,
    out float4 diffuse,
    out float4 spec)
{
    ambient = float4(0, 0, 0, 0);
 diffuse = float4(0, 0, 0, 0);
    spec    = float4(0, 0, 0, 0);

    float3 lightVec = -normalize(LightDirection);

    // Ambient
    ambient = MatAmbient * LightAmbient * LightIntensity;

    float diffuseFactor = dot(lightVec, normal);

    [flatten]
    if (diffuseFactor > 0.0f)
    {
  // Diffuse
    diffuse = diffuseFactor * MatDiffuse * LightDiffuse * LightIntensity;

        // Specular (Phong)
 float3 v = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(v, toEye), 0.0f), MatSpecular.w);
        spec = specFactor * MatSpecular * LightSpecular * LightIntensity;
    }
}

// ===========================================================
// Normal Mapping
// ===========================================================
void ComputeNormalMapping(
    inout float3 normal,
    float3 tangent,
    float2 uv,
    Texture2D normalMap)
{
    float4 map = normalMap.Sample(LinearSampler, uv);
    if (!any(map.rgb))
        return;

    float3 N = normalize(normal);
    float3 T = normalize(tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float3 tangentNormal = map.rgb * 2.0f - 1.0f;
    normal = normalize(mul(tangentNormal, TBN));
}

#endif // _LIGHTING_HLSLI_
