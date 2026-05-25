// Standard_PS.hlsl
// ���� �ȼ� ���̴�  ? MeshRenderer / ModelRenderer / ModelAnimator ���� ���
// Material SRV ����:
//   t0 : DiffuseMap
//   t1 : SpecularMap
//   t2 : NormalMap
//   t3 : ShadowMap
//   t4 : SsaoMap

#include "Lighting.hlsli"
#include "Shadow.hlsli"

// ===========================================================
// Textures (Material SRV, ���� t0~t4)
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
    // ���� ����ȭ �� �븻�� ����
    input.normal = normalize(input.normal);
    ComputeNormalMapping(input.normal, input.tangent, input.uv, NormalMap);

    float3 toEye = normalize(CameraPositionWS() - input.worldPosition);

    // ��ǻ�� �ؽ�ó
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
        float shadowFactor = CalcShadowFactor(ShadowMap, input.shadow);

        float4 ssaoCoord = input.ssao / input.ssao.w;
        float ambientAccess = 1.0f;
        if (UseSsao)
            ambientAccess = SsaoMap.SampleLevel(LinearSampler, ssaoCoord.xy, 0.0f).r;

        float4 A, D, S;
        ComputeAllLights(input.normal, toEye, input.worldPosition, A, D, S);

        float4 ambient = UseSsao ? (ambientAccess * A) : A;
        float4 diffuse = shadowFactor * D;
        float4 spec    = shadowFactor * S;

        litColor = texColor * (ambient + diffuse) + spec;
    }
    else
    {
     // ����Ʈ ���� ��: �ܼ� ambient(0.3) �����Ͽ� ������Ʈ�� ���̵���
        float4 fallbackAmbient = MatAmbient * float4(0.3f, 0.3f, 0.3f, 1.0f);
    litColor = texColor * (fallbackAmbient + MatDiffuse * 0.5f);
    }

    litColor.a = MatDiffuse.a * texColor.a;
    return litColor;
}

// ===========================================================
// PS_Wireframe  ? �ܻ� ���̾������� �����
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
