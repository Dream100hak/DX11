// Lighting.hlsli
// Phong/Blinn-Phong ������ �Լ�
// Common.hlsli �� LightBuffer / MaterialBuffer �� �����

#ifndef _LIGHTING_HLSLI_
#define _LIGHTING_HLSLI_

#include "Common.hlsli"

// ===========================================================
// Directional Light (Blinn-Phong) - ����
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
// Unified Light Data (Directional / Point / Spot)
// type: 0=Directional, 1=Point, 2=Spot
// ===========================================================
struct LightData
{
    float4 diffuse;
    float4 ambient;
    float3 direction;
    float  intensity;
    float3 position;
    float  range;
    float3 attenuation;   // (constant, linear, quadratic)
    float  spotAngle;     // cos(half-angle)
    int    type;          // 0=Directional, 1=Point, 2=Spot
    float3 pad;
};

cbuffer LightArrayBuffer : register(b7)
{
    LightData lights[16];
    int lightCount;
    float3 lightPadding;
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
        ambient += MatAmbient * lights[i].ambient * lights[i].intensity;

        float3 lightVec = -normalize(lights[i].direction);
        float diffuseFactor = dot(lightVec, normal);

        [flatten]
        if (diffuseFactor > 0.0f)
        {
            diffuse += diffuseFactor * MatDiffuse * lights[i].diffuse * lights[i].intensity;
            float3 v = reflect(-lightVec, normal);
            float specFactor = pow(max(dot(v, toEye), 0.0f), MatSpecular.w);
            spec += specFactor * MatSpecular * lights[i].diffuse * lights[i].intensity;
        }
    }
}

void ComputeAllLights(
    float3 normal,
    float3 toEye,
    float3 worldPos,
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
        float3 lightVec;
        float  att = 1.0f;

        if (lights[i].type == 0)
        {
            lightVec = -normalize(lights[i].direction);
        }
        else
        {
            float3 toLight = lights[i].position - worldPos;
            float d = length(toLight);
            if (d > lights[i].range) continue;
            lightVec = toLight / d;
            att = 1.0f / (lights[i].attenuation.x + lights[i].attenuation.y * d + lights[i].attenuation.z * d * d);

            if (lights[i].type == 2)
            {
                float cosAngle = dot(-lightVec, normalize(lights[i].direction));
                float outerCos = lights[i].spotAngle;
                float innerCos = lerp(outerCos, 1.0f, 0.3f);
                float spotFactor = saturate((cosAngle - outerCos) / (innerCos - outerCos));
                att *= spotFactor;
            }
        }

        ambient += MatAmbient * lights[i].ambient * lights[i].intensity * att;

        float NdotL = dot(lightVec, normal);
        [flatten]
        if (NdotL > 0.0f)
        {
            diffuse += NdotL * MatDiffuse * lights[i].diffuse * lights[i].intensity * att;
            float3 v = reflect(-lightVec, normal);
            float specFactor = pow(max(dot(v, toEye), 0.0f), MatSpecular.w);
            spec += specFactor * MatSpecular * lights[i].diffuse * lights[i].intensity * att;
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
