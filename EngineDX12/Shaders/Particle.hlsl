// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

struct Particle { float3 pos; float size; float3 col; float pad; }; // 32B (CPU PInst 와 동일)
StructuredBuffer<Particle> gParts : register(t1); // 루트 SRV(param2)로 바인드 — 입력레이아웃/VB 불필요
cbuffer BillCB : register(b1) { float3 gCamRight; float _p0; float3 gCamUp; float _p1; };
struct VOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float3 col:COLOR; };
VOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    float2 cs[6] = { float2(-1,-1), float2(1,-1), float2(1,1), float2(-1,-1), float2(1,1), float2(-1,1) };
    float2 c = cs[vid];
    Particle p = gParts[iid];
    float3 wp = p.pos + gCamRight * (c.x * p.size) + gCamUp * (c.y * p.size);
    VOut o; o.pos = mul(float4(wp, 1.0), gMVP); o.uv = c; o.col = p.col; return o;
}
float4 PSMain(VOut i) : SV_TARGET
{
    float r = saturate(1.0 - dot(i.uv, i.uv)); // 소프트 원형 폴오프
    return float4(i.col * r, r);               // 가산 블렌드 전제
}
