// SceneGrid.hlsl
// 에디터 씬 그리드 ? 거리 기반 페이드아웃

#include "Common.hlsli"

struct GridVSInput
{
    float4 position : POSITION;
    float2 uv    : TEXCOORD;
};

struct GridOutput
{
    float4 position    : SV_POSITION;
float3 worldPosition : TEXCOORD0;
    float2 uv            : TEXCOORD1;
};

cbuffer GridTransform : register(b1)
{
    matrix GridW;   // TransformBuffer.W 재활용
};

// ── VS ───────────────────────────────────────────────────────
GridOutput VS_Main(GridVSInput input)
{
    GridOutput output;
    output.position      = mul(input.position, W);
    output.worldPosition = output.position.xyz;
  output.position      = mul(output.position, VP);
    output.uv            = input.uv;
 return output;
}

// ── PS ───────────────────────────────────────────────────────
float4 PS_Main(GridOutput input) : SV_TARGET
{
    float3 camPos = CameraPositionWS();
    float  dist   = distance(input.worldPosition, camPos);

    const float maxDist  = 30.f;
    const float fadeDist = 15.f;
 float alpha = saturate((maxDist - dist) / (maxDist - fadeDist));

    float4 color = float4(1.f, 1.f, 1.f, 0.5f * alpha);
    clip(color.a - 0.15f);
    return color;
}
