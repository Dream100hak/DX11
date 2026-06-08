// Thumbnail.hlsl
// �޽� ������ ����Ͽ� - �ܼ� Lambert + Ambient

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

// 모델 프리뷰/썸네일용 — 텍스처 + 단순 조명 (그림자/SSAO/라이트배열 의존 없음).
// FX Standard/Thumbnail 프리뷰 렌더를 HLSL 로 대체 (상태 누수 없음).
float4 PS_PreviewLit(MeshOutput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 L = normalize(float3(0.3f, -1.0f, -0.4f));
    float  ndotl = max(dot(N, -L), 0.f);
    float3 lighting = float3(0.45f, 0.45f, 0.45f) + ndotl * float3(0.85f, 0.85f, 0.85f);

    float4 tex = MatDiffuse;
    if (UseTexture)
        tex = DiffuseMap.Sample(LinearSampler, input.uv);

    return float4(tex.rgb * lighting, 1.f);
}

// ���� PS (�ܻ� / �ؽ�ó ����) ����������������������������������������������������������������������
float4 PS_Solid(MeshOutput input) : SV_TARGET
{
    float3 lightDir   = normalize(float3(0.f, -1.f, 0.f));
    float3 lightColor = float3(0.3f, 0.3f, 0.3f);
    float3 normal     = normalize(input.normal);

    float ndotl = max(dot(normal, -lightDir), 0.f);
    float3 diffuse  = ndotl * lightColor;
    float3 ambient  = float3(0.5f, 0.5f, 0.5f);

    return float4(ambient + diffuse, 1.f);
}

// ���� PS (���̾������� ��������) ����������������������������������������������������������������
float4 PS_Wireframe(MeshOutput input) : SV_TARGET
{
    float3 lightDir   = normalize(float3(0.f, -1.f, 0.f));
    float3 lightColor = float3(0.3f, 0.3f, 0.3f);
    float3 normal = normalize(input.normal);

    float ndotl = max(dot(normal, -lightDir), 0.f);
    float3 diffuse  = ndotl * lightColor;
    float3 ambient= float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambient + diffuse;

    // ���̾������� ���� 50% ȥ��
    float3 wireColor = float3(1.f, 1.f, 1.f);
 finalColor = lerp(finalColor, wireColor, 0.5f);

    return float4(finalColor, 1.f);
}
