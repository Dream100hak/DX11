// Sky_VS.hlsl + Sky_PS.hlsl
// ��ī�̹ڽ� : w=0 ���� ī�޶� ��ġ ���� ���� ����

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0);

struct SkyVSInput
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct SkyOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

// ���� VS ��������������������������������������������������������������������������������������������������������������
SkyOutput VS_Main(SkyVSInput input)
{
    SkyOutput output;
    // w=0 �� �̵� ����, ī�޶� �߽ɿ� ����
    float4 viewPos = mul(float4(input.position.xyz, 0.0f), V);
    float4 clipPos = mul(viewPos, P);
    // z=w �� ���� �� �׻� far plane (depth=1)
    output.position = clipPos.xyww;
    output.uv   = input.uv;
    return output;
}

// ���� PS ��������������������������������������������������������������������������������������������������������������
float4 PS_Main(SkyOutput input) : SV_TARGET
{
    float4 color = DiffuseMap.Sample(LinearSampler, input.uv);
    // HDR sceneColor 는 linear 공간 — 감마 텍스처 선형화 (톤매핑 패스가 재인코딩)
    color.rgb = pow(abs(color.rgb), 2.2f);
    return color;
}
