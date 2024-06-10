#ifndef _LIGHT_FX_
#define _LIGHT_FX_

#include "00. Global.fx"

////////////
// Struct //
////////////

struct DirLightDesc
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 emissive;
	float3 direction;
    float intensity;
};

struct PointLightDesc
{
    float4 ambient;
    float4 diffuse;
    float4 specular;

    float3 Position;
    float Range;

    float3 Att;
};

struct SpotLightDesc
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;

    float3 Position;
    float Range;

    float3 Direction;
    float Spot;

    float3 Att;
};

struct MaterialDesc
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 emissive;
    int lightCount;
    int useTexture;
    int useAlphaclip;
    int useSsao;
};

/////////////////
// ConstBuffer //
/////////////////

cbuffer LightBuffer
{
    DirLightDesc GlobalLight;
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
Texture2D ShadowMap;
Texture2D SsaoMap;

//////////////
// Function //
//////////////


void ComputeDirectionalLight(float3 normal, float3 toEye,                          
					         out float4 ambient,
						     out float4 diffuse,
						     out float4 spec)
{
	// Initialize outputs.
    ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

	// The light vector aims opposite the direction the light rays travel.
    float3 lightVec = -GlobalLight.direction;

	// Add ambient term.
    ambient = Material.ambient * GlobalLight.ambient * GlobalLight.intensity;
    
    
    float diffuseFactor = dot(lightVec, normal);

	// Flatten to avoid dynamic branching.
	[flatten]
    if (diffuseFactor > 0.0f)
    {
        float3 v = reflect(-lightVec, normal);
        float specFactor = pow(max(dot(v, toEye), 0.0f), Material.specular.w);
					
        diffuse = diffuseFactor * Material.diffuse * GlobalLight.diffuse * GlobalLight.intensity;
        spec = specFactor * Material.specular * GlobalLight.specular * GlobalLight.intensity;
    }
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

//---------------------------------------------------------------------------------------
// Performs shadowmap test to determine if a pixel is in shadow.
//---------------------------------------------------------------------------------------

static const float SMAP_SIZE = 2048.0f;
static const float SMAP_DX = 1.0f / SMAP_SIZE;

float CalcShadowFactor(SamplerComparisonState samShadow,
	Texture2D shadowMap,
	float4 shadowPos)
{
	// Complete projection by doing division by w.
    shadowPos.xyz /= shadowPos.w;

	// Depth in NDC space.
    float depth = shadowPos.z;

	// Texel size.
    const float dx = SMAP_DX;

	//return shadowMap.SampleCmpLevelZero(samShadow, shadowPosH.xy, depth).r;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

	[unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			shadowPos.xy + offsets[i], depth).r;
    }

    return percentLit /= 9.0f;
}

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
#endif

