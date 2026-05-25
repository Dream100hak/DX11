// GBuffer_PS.hlsl
// G-Buffer fill pixel shader - outputs to 3 MRT
// Paired with Standard_VS.hlsl (VS_Mesh / VS_Model / VS_Animation)
//
// SV_Target0 : Albedo (RGBA8)
// SV_Target1 : World Normal (RGBA16F) - packed [0,1]
// SV_Target2 : World Position (RGBA16F)

#include "Lighting.hlsli"

Texture2D DiffuseMap : register(t0);
Texture2D NormalMap  : register(t2);

struct GBufferOutput
{
    float4 albedo   : SV_Target0;
    float4 normal   : SV_Target1;
    float4 position : SV_Target2;
};

GBufferOutput PS_GBuffer(MeshOutput input)
{
    GBufferOutput output;

    input.normal = normalize(input.normal);
    ComputeNormalMapping(input.normal, input.tangent, input.uv, NormalMap);

    float4 texColor = MatDiffuse;
    if (UseTexture)
    {
        texColor = DiffuseMap.Sample(LinearSampler, input.uv);
        if (UseAlphaClip)
            clip(texColor.a - 0.1f);
    }

    output.albedo   = texColor;
    output.normal   = float4(input.normal * 0.5f + 0.5f, MatSpecular.w / 256.0f);
    output.position = float4(input.worldPosition, 1.0f);

    return output;
}
