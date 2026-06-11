// CubeMap.hlsl
// 큐브맵 큐브텍스처 (EditorTool)

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
    float3 PosL : TEXCOORD0;   // 로컬 공간 을 큐브텍스처 샘플링 위치
};

// ===========================================================
// 버텍스 셰이더
// ===========================================================
CubeOut VS_Main(CubeVSIn input)
{
    CubeOut output;
    // w=0 으로 이동 차단, xyww 로 z=w (far plane)
    output.PosH = mul(float4(input.position.xyz, 0.f), VP).xyww;
    output.PosL = input.position.xyz;
    return output;
}

// ===========================================================
// 픽셀 셰이더
// ===========================================================
float4 PS_Main(CubeOut input) : SV_TARGET
{
    float4 color = CubeMapTex.Sample(LinearSampler, input.PosL);
    // HDR sceneColor 를 linear 공간 에서 감마 트랜스폼 (톤매핑 패스가 인코딩)
    color.rgb = pow(abs(color.rgb), 2.2f);
    return color;
}
