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
// 캐스케이드 배열 슬라이스 PCF 9-tap
//  shadowMap : 캐스케이드 Texture2DArray, slice : 캐스케이드 인덱스
//  shadowPos : worldPos 를 해당 캐스케이드 V*P*T 로 변환한 동차좌표
float CalcShadowFactorArray(Texture2DArray shadowMap, int slice, float4 shadowPos, float bias)
{
    shadowPos.xyz /= shadowPos.w;

    float depth = shadowPos.z - bias; // 섀도 아크네(지글지글 줄무늬) 방지용 깊이 바이어스
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
            float3(shadowPos.xy + offsets[i], (float)slice),
            depth).r;
    }

    return percentLit / 9.0f;
}

// 바이어스 없는 오버로드 (CSM 등 기존 호출 호환)
float CalcShadowFactorArray(Texture2DArray shadowMap, int slice, float4 shadowPos)
{
    return CalcShadowFactorArray(shadowMap, slice, shadowPos, 0.0f);
}

// 레거시 단일 섀도우 호환 — 캐스케이드 0 슬라이스 샘플 (포워드/터레인 포워드용)
float CalcShadowFactor(Texture2DArray shadowMap, float4 shadowPos)
{
    return CalcShadowFactorArray(shadowMap, 0, shadowPos);
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
