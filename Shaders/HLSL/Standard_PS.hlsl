// Standard_PS.hlsl
// 공통 픽셀 셰이더  ? MeshRenderer / ModelRenderer / ModelAnimator 전부 사용
// Material SRV 슬롯:
//   t0 : DiffuseMap
//   t1 : SpecularMap
//   t2 : NormalMap
//   t3 : ShadowMap
//   t4 : SsaoMap

#include "Lighting.hlsli"
#include "Shadow.hlsli"

// ===========================================================
// Textures (Material SRV, 슬롯 t0~t4)
// ===========================================================
Texture2D DiffuseMap  : register(t0);
Texture2D SpecularMap : register(t1);
Texture2D NormalMap   : register(t2);
Texture2D ShadowMap : register(t3);
Texture2D SsaoMap     : register(t4);

// ===========================================================
// PS_Main
// ===========================================================
float4 PS_Main(MeshOutput input) : SV_TARGET
{
    // 법선 정규화 및 노말맵 적용
    input.normal = normalize(input.normal);
    ComputeNormalMapping(input.normal, input.tangent, input.uv, NormalMap);

    float3 toEye = normalize(CameraPositionWS() - input.worldPosition);

    // 디퓨즈 텍스처
    float4 texColor = MatDiffuse;
    if (UseTexture)
    {
        texColor = DiffuseMap.Sample(LinearSampler, input.uv);

        if (UseAlphaClip)
   clip(texColor.a - 0.1f);
    }

    float4 litColor = texColor;

    if (lightCount > 0)
    {
        float4 ambient = float4(0, 0, 0, 0);
        float4 diffuse = float4(0, 0, 0, 0);
    float4 spec    = float4(0, 0, 0, 0);

     float shadowFactor = CalcShadowFactor(ShadowMap, input.shadow);

  float4 ssaoCoord = input.ssao / input.ssao.w;
        float ambientAccess = 1.0f;
        if (UseSsao)
 ambientAccess = SsaoMap.SampleLevel(LinearSampler, ssaoCoord.xy, 0.0f).r;

      float4 A, D, S;
        ComputeDirectionalLightArray(input.normal, toEye, A, D, S);

        ambient = UseSsao ? (ambientAccess * A) : A;
        diffuse = shadowFactor * D;
        spec    = shadowFactor * S;

        litColor = texColor * (ambient + diffuse) + spec;
    }
    else
    {
     // 라이트 없을 때: 단순 ambient(0.3) 적용하여 오브젝트가 보이도록
        float4 fallbackAmbient = MatAmbient * float4(0.3f, 0.3f, 0.3f, 1.0f);
    litColor = texColor * (fallbackAmbient + MatDiffuse * 0.5f);
    }

    litColor.a = MatDiffuse.a * texColor.a;
    return litColor;
}

// ===========================================================
// PS_Wireframe  ? 단색 와이어프레임 디버그
// ===========================================================
float4 PS_Wireframe(MeshOutput input) : SV_TARGET
{
    float3 cameraPos   = CameraPositionWS();
    float  distToCamera = distance(input.worldPosition, cameraPos);

    float maxDist   = 30.0f;
    float fadeDist  = 15.0f;
    float alpha     = saturate((maxDist - distToCamera) / (maxDist - fadeDist));

    float4 color = float4(1.f, 1.f, 1.f, 0.5f);
    color.a *= alpha;

    clip(color.a - 0.15f);
    return color;
}
