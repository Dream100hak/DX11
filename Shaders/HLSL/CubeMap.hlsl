// CubeMap.hlsl
// 큐브맵 스카이박스 (EditorTool)

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
    float3 PosL : TEXCOORD0;   // 로컬 방향 → 큐브맵 샘플링에 사용
};

// ── VS ───────────────────────────────────────────────────────
CubeOut VS_Main(CubeVSIn input)
{
    CubeOut output;
    // w=0 → 이동 없음, xyww → z=w (far plane)
    output.PosH = mul(float4(input.position.xyz, 0.f), VP).xyww;
    output.PosL = input.position.xyz;
    return output;
}

// ── PS ───────────────────────────────────────────────────────
float4 PS_Main(CubeOut input) : SV_TARGET
{
return CubeMapTex.Sample(LinearSampler, input.PosL);
}
