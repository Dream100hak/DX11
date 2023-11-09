#ifndef _LIGHT_FX_
#define _LIGHT_FX_

#include "00. Global.fx"

////////////
// Struct //
////////////

struct LightDesc
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 emissive;
	float3 direction;
    float intensity;
};

struct MaterialDesc
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 emissive;

};

/////////////////
// ConstBuffer //
/////////////////

cbuffer LightBuffer
{
	LightDesc GlobalLight;
};

cbuffer MaterialBuffer
{
	MaterialDesc Material;
};

/////////
// SRV //
/////////

Texture2D DiffuseMap;
Texture2D SpecularMap;
Texture2D NormalMap;
TextureCube CubeMap;

//////////////
// Function //
//////////////

float4 ComputeLight(float3 normal, float2 uv, float3 worldPosition)
{
    float4 ambientColor = GlobalLight.ambient * Material.ambient * GlobalLight.intensity;

    // Diffuse calculation
    float ndotl = dot(-GlobalLight.direction, normalize(normal));
    float4 diffuse = DiffuseMap.Sample(LinearSampler, uv);
    float4 diffuseColor = diffuse * ndotl * GlobalLight.diffuse * Material.diffuse * GlobalLight.intensity;

    // Specular calculation
    float3 R = normalize(reflect(-GlobalLight.direction, normal));
    float3 V = normalize(CameraPosition() - worldPosition);
    float specAngle = max(dot(R, V), 0.0f);
    float4 specularColor = pow(specAngle, 3) * GlobalLight.specular * Material.specular * GlobalLight.intensity;

    // Emissive component does not usually depend on light intensity
    float4 emissiveColor = Material.emissive;

    // Combine all components
    float4 color = ambientColor + diffuseColor + specularColor + emissiveColor;

    // Clamp to [0, 1] range to avoid overflow in LDR
    return saturate(color);
}

void ComputeNormalMapping(inout float3 normal, float3 tangent, float2 uv)
{
	// [0,255] 범위에서 [0,1]로 변환
	float4 map = NormalMap.Sample(LinearSampler, uv);
	if (any(map.rgb) == false)
		return;

	float3 N = normalize(normal); // z
	float3 T = normalize(tangent); // x
	float3 B = normalize(cross(N, T)); // y
	float3x3 TBN = float3x3(T, B, N); // TS -> WS

	// [0,1] 범위에서 [-1,1] 범위로 변환
	float3 tangentSpaceNormal = (map.rgb * 2.0f - 1.0f);
	float3 worldNormal = mul(tangentSpaceNormal, TBN);

	normal = worldNormal;
}

#endif

