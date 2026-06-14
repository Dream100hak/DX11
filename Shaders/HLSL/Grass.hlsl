// Grass.hlsl
// 터레인 식생(잔디) — 인스턴스 쿼드를 GBuffer 로 그린다(디퍼드 라이팅/그림자/SSAO 일괄 적용).
//   slot0: VertexTextureNormal(quad)  slot1: InstancingData(world+picked)
//   b0: GlobalBuffer  b8: GrassBuffer(바람/시간)
// 텍스처 의존 없음 — UV 기반 절차적 블레이드 알파 + 그린 그라디언트(인스턴스별 변이).

#include "Common.hlsli"

cbuffer GrassBuffer : register(b8)
{
    float GameTime;
    float WindStrength;
    float WindFreq;
    float MaxDist;      // 이 거리에서 완전히 사라짐
    float FadeRange;    // MaxDist 앞 페이드 구간 (블레이드 축소)
    float3 GrassPad;
};

struct VSInput
{
    float4 position : POSITION;   // 쿼드 로컬 (y: 0=밑동, 1=끝)
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    // Instancing (InputSlot 1)
    uint   instanceID : SV_InstanceID;
    matrix world      : INST_WORLD;
    uint   isPicked   : PICKED;
};

struct GBufferOutput
{
    float4 albedo   : SV_Target0; // rgb=albedo(linear), a=metallic
    float4 normal   : SV_Target1; // rgb=packed normal, a=roughness
    float4 position : SV_Target2; // xyz=worldPos, w=1
    float4 emissive : SV_Target3;
};

MeshOutput VS_Grass(VSInput input)
{
    MeshOutput o = (MeshOutput)0;

    // 거리 페이드 — 인스턴스 밑동(월드 평행이동)까지 거리로 블레이드를 0 까지 축소(원거리 자연 소멸)
    float3 basePos = float3(input.world._41, input.world._42, input.world._43);
    float  dist = distance(CameraPositionWS(), basePos);
    float  fade = saturate((MaxDist - dist) / max(FadeRange, 0.001f));

    // 끝으로 갈수록 좁아지는 가중치는 원본 로컬 y 기준 (페이드 축소 전)
    float heightW = saturate(input.position.y);

    // 밑동(로컬 원점) 기준으로 fade 만큼 축소 → fade=0 이면 한 점으로 붕괴(비가시)
    float3 localScaled = input.position.xyz * fade;
    float4 wpos = mul(float4(localScaled, 1.0f), input.world);
    float phase = wpos.x * 0.35f + wpos.z * 0.45f;
    float sx = sin(GameTime * WindFreq + phase);
    float sz = cos(GameTime * WindFreq * 0.7f + phase * 1.3f);
    wpos.x += sx * WindStrength * heightW;
    wpos.z += sz * WindStrength * heightW * 0.6f;

    o.worldPosition = wpos.xyz;
    o.position      = mul(wpos, VP);
    o.uv            = input.uv;
    o.normal        = float3(0.0f, 1.0f, 0.0f); // 위쪽 노멀 — 잔디 양면 어둠 방지(부드러운 라이팅)
    o.tangent       = float3(1.0f, 0.0f, 0.0f);
    o.picked        = input.isPicked;
    return o;
}

GBufferOutput PS_Grass(MeshOutput input)
{
    float2 uv = input.uv;

    // 절차적 블레이드 알파 — 끝으로 갈수록 좁아지는 삼각형 실루엣
    float halfW = lerp(0.5f, 0.06f, uv.y);
    float edge  = abs(uv.x - 0.5f);
    clip(halfW - edge); // 블레이드 밖은 버림 (알파컷)

    // 그린 그라디언트(밑동 어둡고 끝 밝음) + 인스턴스별 색 변이
    float3 baseCol = float3(0.10f, 0.30f, 0.05f);
    float3 tipCol  = float3(0.45f, 0.70f, 0.18f);
    float3 col = lerp(baseCol, tipCol, uv.y);
    float h = frac(sin(dot(floor(input.worldPosition.xz), float2(12.9898f, 78.233f))) * 43758.5453f);
    col *= (0.75f + 0.5f * h);

    GBufferOutput o;
    o.albedo   = float4(pow(abs(col), 2.2f), 0.0f);                  // metallic 0
    o.normal   = float4(normalize(input.normal) * 0.5f + 0.5f, 0.9f); // roughness 0.9
    o.position = float4(input.worldPosition, 1.0f);
    o.emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return o;
}
