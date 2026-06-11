// GBuffer_PS.hlsl
// G-Buffer fill pixel shader - outputs to 3 MRT
// Paired with Standard_VS.hlsl (VS_Mesh / VS_Model / VS_Animation)
//
// SV_Target0 : Albedo.rgb + Metallic (RGBA8)
// SV_Target1 : World Normal packed [0,1] + Roughness (RGBA16F)
// SV_Target2 : World Position + mask (RGBA16F, w=1 → 유효 GBuffer 픽셀)

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

    // MatDiffuse 는 틴트로 곱함 — 대체해 버리면 인스펙터 Diffuse 색이 텍스처 머티리얼에 안 먹음
    float4 texColor = MatDiffuse;
    if (UseTexture)
    {
        texColor = DiffuseMap.Sample(LinearSampler, input.uv) * MatDiffuse;
        if (UseAlphaClip)
            clip(texColor.a - 0.1f);
    }

    // sRGB(감마) 텍스처/컬러 → linear (라이팅은 linear 공간, 톤매핑 패스가 감마 인코딩)
    output.albedo   = float4(pow(abs(texColor.rgb), 2.2f), Metallic);
    output.normal   = float4(input.normal * 0.5f + 0.5f, Roughness);
    output.position = float4(input.worldPosition, 1.0f);

    return output;
}
