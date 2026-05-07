// Outline_PS.hlsl
// 단색 아웃라인 출력

#include "Common.hlsli"

cbuffer OutlineBuffer : register(b7)
{
    float4 OutlineColor;
    float  OutlineWidth;
    float3 OutlinePad;
};

float4 PS_Main(float4 pos : SV_POSITION) : SV_TARGET
{
    return OutlineColor;
}
