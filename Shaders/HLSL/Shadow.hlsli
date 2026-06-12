// Shadow.hlsli
// PCF 9-tap 소프트 섀도우 팩터 계산

#ifndef _SHADOW_HLSLI_
#define _SHADOW_HLSLI_

#include "Common.hlsli"

// ===========================================================
// Shadow Map 파라미터
// ===========================================================
static const float SMAP_SIZE = 2048.0f;
static const float SMAP_DX   = 1.0f / SMAP_SIZE;

// ===========================================================
// CalcShadowFactor
//  shadowPos : VS 에서 Light 공간으로 변환된 위치 (동차좌표)
// ===========================================================
float CalcShadowFactor(Texture2D shadowMap, float4 shadowPos)
{
// Perspective divide
    shadowPos.xyz /= shadowPos.w;

    float depth = shadowPos.z;
    const float dx = SMAP_DX;

  const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx,  0.0f), float2(0.0f,  0.0f), float2(dx,  0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    float percentLit = 0.0f;
    [unroll]
 for (int i = 0; i < 9; ++i)
    {
     percentLit += shadowMap.SampleCmpLevelZero(
       ShadowSampler,
   shadowPos.xy + offsets[i],
      depth).r;
    }

    return percentLit / 9.0f;
}

// ===========================================================
// Shadow 좌표 계산 (VS 에서 호출)
//  worldPos : 월드 공간 버텍스 위치
// ===========================================================
float4 CalcShadowCoord(float3 worldPos)
{
    return mul(float4(worldPos, 1.0f), Shadow);
}

#endif // _SHADOW_HLSLI_
