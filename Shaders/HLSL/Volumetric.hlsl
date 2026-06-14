// Volumetric.hlsl
// 볼류메트릭 라이트(갓레이) — 디퍼드 라이팅 직후 풀스크린.
// 카메라→씬(또는 원거리) 레이마치하며 CSM 그림자로 태양 가시성을 샘플,
// Henyey-Greenstein 위상함수로 인스캐터링을 누적해 sceneColor 에 가산.
//   t0: SceneColor  t1: WorldPos(.w=mask)  t2: CSM ShadowMap(Texture2DArray)
//   b0: Global  b9: CascadeBuffer  b10: VolumetricBuffer

#include "Common.hlsli"
#include "Shadow.hlsli"

Texture2D      SceneColor      : register(t0);
Texture2D      GBufferPosition : register(t1);
Texture2DArray ShadowMap       : register(t2);

cbuffer CascadeBuffer : register(b9)
{
    matrix CascadeVPT[4];
    float4 CascadeSplits;
    int    CascadeCount;
    int    CascadePadA;
    float2 CascadePadB;
};

cbuffer VolumetricBuffer : register(b10)
{
    float3 SunDir;        // 라이트 진행 방향(빛이 나아가는 쪽)
    float  SunIntensity;
    float3 SunColor;
    float  FogDensity;    // 인스캐터링 밀도
    float  ScatterG;      // HG 비등방성 (0=등방, 0.7=전방산란)
    float  StepCount;
    float  MaxDistance;   // 레이마치 최대 거리
    float  VolPad;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

VSOut VS_Main(uint vertexID : SV_VertexID)
{
    VSOut o;
    o.uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.position = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

// Henyey-Greenstein 위상함수
float HenyeyGreenstein(float cosT, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * 3.14159265f * pow(abs(1.0f + g2 - 2.0f * g * cosT), 1.5f));
}

int SelectCascade(float viewZ)
{
    int c = CascadeCount - 1;
    [unroll]
    for (int i = 0; i < 4; ++i)
        if (i < CascadeCount && viewZ <= CascadeSplits[i]) { c = i; break; }
    return c;
}

float4 PS_Main(VSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 scene = SceneColor.Sample(LinearSampler, uv).rgb;

    // uv → 월드 레이 방향 (P/VInv 로 복원, VPInv 불필요)
    float2 ndc = uv * 2.0f - 1.0f; ndc.y = -ndc.y;
    float3 viewDir = float3(ndc.x / P._11, ndc.y / P._22, 1.0f);
    float3 rayDir = normalize(mul(viewDir, (float3x3)VInv));

    float3 camPos = CameraPositionWS();

    // 지오메트리면 표면까지, 스카이면 MaxDistance 까지 마치
    float4 posS = GBufferPosition.Sample(PointSampler, uv);
    float marchDist = (posS.w > 0.5f) ? min(length(posS.xyz - camPos), MaxDistance) : MaxDistance;

    int steps = (int)StepCount;
    float stepLen = marchDist / max(1, steps);

    // 픽셀별 디더로 밴딩 완화
    float dither = frac(sin(dot(uv, float2(12.9898f, 78.233f))) * 43758.5453f);

    float cosT = dot(rayDir, -normalize(SunDir)); // 시선과 태양방향 사이각
    float phase = HenyeyGreenstein(cosT, ScatterG);

    float accum = 0.0f;
    float3 p = camPos + rayDir * stepLen * dither;
    [loop]
    for (int i = 0; i < steps; ++i)
    {
        float viewZ = mul(float4(p, 1.0f), V).z;
        int c = SelectCascade(viewZ);
        float4 sp = mul(float4(p, 1.0f), CascadeVPT[c]);
        float lit = CalcShadowFactorArray(ShadowMap, c, sp); // 태양 가시성(1=빛)
        accum += lit * FogDensity * stepLen;
        p += rayDir * stepLen;
    }

    float3 scatter = SunColor * SunIntensity * phase * accum;
    return float4(scene + scatter, 1.0f);
}
