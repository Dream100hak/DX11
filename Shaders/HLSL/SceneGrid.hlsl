// SceneGrid.hlsl
// 씬 에디터 그리드 — 카메라 거리 페이드 + 월드 축 컬러 (라인 리스트)

#include "Common.hlsli"

cbuffer GridParamsBuffer : register(b8)
{
    float FadeStart;   // 페이드 시작 거리 (m)
    float FadeEnd;     // 완전히 사라지는 거리 (m)
    float BaseAlpha;   // 라인 기본 농도
    float GridPad;
};

struct GridVSInput
{
    float4 position : POSITION;   // 정점은 float3, IA가 w=1 충전
    float2 uv       : TEXCOORD;
};

struct GridOutput
{
    float4 position      : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

GridOutput VS_Main(GridVSInput input)
{
    GridOutput output;
    output.position      = mul(input.position, W);   // b1 TransformBuffer.W
    output.worldPosition = output.position.xyz;
    output.position      = mul(output.position, VP);  // b0 GlobalBuffer.VP
    return output;
}

float4 PS_Main(GridOutput input) : SV_TARGET
{
    float3 camPos = CameraPositionWS();
    float  dist   = distance(input.worldPosition, camPos);
    float  fade   = saturate((FadeEnd - dist) / (FadeEnd - FadeStart));

    // HDR sceneColor 에 블렌드 후 ACES 톤매핑되므로 어두운 라인 + 높은 알파로 대비 확보
    // 월드 축 강조 — X축(z=0) 빨강, Z축(x=0) 파랑 (유니티 스타일)
    float3 rgb   = float3(0.05f, 0.05f, 0.05f);
    float  alpha = BaseAlpha;
    if (abs(input.worldPosition.z) < 0.02f)
    {
        rgb   = float3(1.2f, 0.15f, 0.15f);
        alpha = max(alpha, 0.9f);
    }
    else if (abs(input.worldPosition.x) < 0.02f)
    {
        rgb   = float3(0.2f, 0.45f, 1.6f);
        alpha = max(alpha, 0.9f);
    }

    float4 color = float4(rgb, alpha * fade);
    clip(color.a - 0.01f);
    return color;
}
