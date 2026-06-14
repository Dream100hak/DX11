// Tree.hlsl
// 터레인 식생(나무) — 인스턴스 저폴리 메시(줄기+원뿔 캐노피)를 GBuffer 로.
//   uv.x = 머티리얼 ID (0=줄기 갈색, 1=캐노피 초록)  uv.y = 미사용
//   캐노피(uv.x≈1)만 약하게 바람 흔들림.

#include "Common.hlsli"

cbuffer GrassBuffer : register(b8) // 잔디와 동일 레이아웃 재사용
{
    float GameTime;
    float WindStrength;
    float WindFreq;
    float MaxDist;
    float FadeRange;
    float3 GrassPad;
};

struct VSInput
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    uint   instanceID : SV_InstanceID;
    matrix world      : INST_WORLD;
    uint   isPicked   : PICKED;
};

struct GBufferOutput
{
    float4 albedo   : SV_Target0;
    float4 normal   : SV_Target1;
    float4 position : SV_Target2;
    float4 emissive : SV_Target3;
};

MeshOutput VS_Tree(VSInput input)
{
    MeshOutput o = (MeshOutput)0;

    float3 basePos = float3(input.world._41, input.world._42, input.world._43);
    float  dist = distance(CameraPositionWS(), basePos);
    float  fade = saturate((MaxDist - dist) / max(FadeRange, 0.001f));

    // 거리 페이드 — 밑동 기준 축소
    float3 localScaled = input.position.xyz * fade;
    float4 wpos = mul(float4(localScaled, 1.0f), input.world);

    // 캐노피(uv.x≈1)만 약하게 흔들림 (줄기는 고정)
    float canopy = saturate(input.uv.x);
    float phase = basePos.x * 0.3f + basePos.z * 0.4f;
    wpos.x += sin(GameTime * WindFreq * 0.5f + phase) * WindStrength * 0.4f * canopy;
    wpos.z += cos(GameTime * WindFreq * 0.4f + phase) * WindStrength * 0.3f * canopy;

    o.worldPosition = wpos.xyz;
    o.position      = mul(wpos, VP);
    o.uv            = input.uv;
    // 노멀은 인스턴스 회전 적용 (균일 스케일 가정)
    o.normal        = normalize(mul(input.normal, (float3x3)input.world));
    o.tangent       = float3(1.0f, 0.0f, 0.0f);
    o.picked        = input.isPicked;
    return o;
}

GBufferOutput PS_Tree(MeshOutput input)
{
    // 줄기(갈색) ↔ 캐노피(초록)
    float3 trunk  = float3(0.30f, 0.18f, 0.08f);
    float3 canopy = float3(0.16f, 0.42f, 0.10f);
    float3 col = lerp(trunk, canopy, saturate(input.uv.x));
    float hsh = frac(sin(dot(floor(input.worldPosition.xz), float2(12.9898f, 78.233f))) * 43758.5453f);
    col *= (0.8f + 0.4f * hsh);

    GBufferOutput o;
    o.albedo   = float4(pow(abs(col), 2.2f), 0.0f);
    o.normal   = float4(normalize(input.normal) * 0.5f + 0.5f, 0.9f);
    o.position = float4(input.worldPosition, 1.0f);
    o.emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return o;
}
