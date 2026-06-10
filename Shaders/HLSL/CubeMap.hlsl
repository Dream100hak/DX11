// CubeMap.hlsl
// ť��� ��ī�̹ڽ� (EditorTool)

#include "Common.hlsli"

TextureCube CubeMapTex : register(t0);

struct CubeVSIn
{
  float4 position : POSITION;
 float2 uv : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct CubeOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : TEXCOORD0;   // ���� ���� �� ť��� ���ø��� ���
};

// ���� VS ��������������������������������������������������������������������������������������������������������������
CubeOut VS_Main(CubeVSIn input)
{
    CubeOut output;
    // w=0 �� �̵� ����, xyww �� z=w (far plane)
    output.PosH = mul(float4(input.position.xyz, 0.f), VP).xyww;
    output.PosL = input.position.xyz;
    return output;
}

// ���� PS ��������������������������������������������������������������������������������������������������������������
float4 PS_Main(CubeOut input) : SV_TARGET
{
    float4 color = CubeMapTex.Sample(LinearSampler, input.PosL);
    // HDR sceneColor 는 linear 공간 — 감마 텍스처 선형화 (톤매핑 패스가 재인코딩)
    color.rgb = pow(abs(color.rgb), 2.2f);
    return color;
}
