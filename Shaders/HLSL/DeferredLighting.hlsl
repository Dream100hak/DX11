// DeferredLighting.hlsl
// Fullscreen deferred lighting pass
// Reads G-Buffer textures and computes multi-light Phong shading
//
// G-Buffer SRV layout:
//   t0 : Albedo (RGBA8)
//   t1 : Normal (RGBA16F, packed [0,1])
//   t2 : Position (RGBA16F, world space)
//   t3 : ShadowMap (from forward pipeline)

#include "Common.hlsli"
#include "Shadow.hlsli"

// G-Buffer textures
Texture2D GBufferAlbedo   : register(t0);
Texture2D GBufferNormal   : register(t1);
Texture2D GBufferPosition : register(t2);
Texture2D ShadowMap       : register(t3);

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

float4 PS_Main(LightingVSOutput input) : SV_TARGET
{
    float4 albedo      = GBufferAlbedo.Sample(PointSampler, input.uv);
    float4 normalPacked = GBufferNormal.Sample(PointSampler, input.uv);
    float4 positionData = GBufferPosition.Sample(PointSampler, input.uv);

    if (positionData.w < 0.5f)
        discard;

    float3 N = normalize(normalPacked.xyz * 2.0f - 1.0f);
    float  specPower = normalPacked.w * 256.0f;
    float3 worldPos  = positionData.xyz;
    float3 toEye     = normalize(CameraPositionWS() - worldPos);

    float shadowFactor = 1.0f;
    float4 shadowHomoPos = mul(float4(worldPos, 1.0f), Shadow);
    float3 shadowNDC = shadowHomoPos.xyz / shadowHomoPos.w;
    if (shadowNDC.x > 0.01f && shadowNDC.x < 0.99f &&
        shadowNDC.y > 0.01f && shadowNDC.y < 0.99f &&
        shadowNDC.z > 0.0f  && shadowNDC.z < 1.0f)
    {
        shadowFactor = CalcShadowFactor(ShadowMap, shadowHomoPos);
    }

    float4 totalAmbient  = float4(0, 0, 0, 0);
    float4 totalDiffuse  = float4(0, 0, 0, 0);
    float4 totalSpecular = float4(0, 0, 0, 0);

    [unroll(16)]
    for (int i = 0; i < lightCount; ++i)
    {
        float3 lightVec;
        float att = 1.0f;

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
                att *= saturate((cosAngle - outerCos) / (innerCos - outerCos));
            }
        }

        totalAmbient += MatAmbient * lights[i].ambient * lights[i].intensity * att;

        float NdotL = dot(lightVec, N);
        [flatten]
        if (NdotL > 0.0f)
        {
            totalDiffuse += NdotL * MatDiffuse * lights[i].diffuse * lights[i].intensity * att;
            float3 reflectVec = reflect(-lightVec, N);
            float specFactor = pow(max(dot(reflectVec, toEye), 0.0f), specPower);
            totalSpecular += specFactor * MatSpecular * lights[i].diffuse * lights[i].intensity * att;
        }
    }

    if (lightCount == 0)
    {
        totalAmbient = MatAmbient * float4(0.3f, 0.3f, 0.3f, 1.0f);
        totalDiffuse = MatDiffuse * 0.5f;
    }

    float4 litColor = albedo * (totalAmbient + shadowFactor * totalDiffuse) + shadowFactor * totalSpecular;
    litColor.a = 1.0f;
    return litColor;
}
