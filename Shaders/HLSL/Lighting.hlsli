// Lighting.hlsli
// Phong/Blinn-Phong 라이팅 함수
// Common.hlsli 와 LightBuffer / MaterialBuffer 를 사용함

#ifndef _LIGHTING_HLSLI_
#define _LIGHTING_HLSLI_

#include "Common.hlsli"

// ===========================================================
// Directional Light (Blinn-Phong) - 단일
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
// Directional Light Array (Blinn-Phong) - 다중
// ===========================================================
// 구조체 정의
struct DirectionalLightData
{
    float4 diffuse;
    float4 ambient;
    float intensity;
    float3 direction;
};

cbuffer LightArrayBuffer : register(b7)
{
    DirectionalLightData lights[16];  // ← 고정 크기 (MAX_LIGHTS 매크로 불가)
    int lightCount;
    float3 padding;
};

void ComputeDirectionalLightArray(
    float3 normal,
    float3 toEye,
    out float4 ambient,
    out float4 diffuse,
    out float4 spec)
{
    ambient = float4(0, 0, 0, 0);
    diffuse = float4(0, 0, 0, 0);
    spec    = float4(0, 0, 0, 0);

    [unroll(16)]
  for (int i = 0; i < lightCount; ++i)
    {
        float3 lightVec = -normalize(lights[i].direction);
        
        // Ambient
     ambient += MatAmbient * lights[i].ambient * lights[i].intensity;

   float diffuseFactor = dot(lightVec, normal);

        [flatten]
        if (diffuseFactor > 0.0f)
        {
            // Diffuse
        diffuse += diffuseFactor * MatDiffuse * lights[i].diffuse * lights[i].intensity;

   // Specular (Phong)
            float3 v = reflect(-lightVec, normal);
     float specFactor = pow(max(dot(v, toEye), 0.0f), MatSpecular.w);
         spec += specFactor * MatSpecular * lights[i].diffuse * lights[i].intensity;
      }
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
