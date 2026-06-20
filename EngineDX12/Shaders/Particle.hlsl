// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

struct Particle { float3 pos; float size; float3 col; float soft; }; // 32B (CPU PInst 와 동일). soft=폴오프 지수
StructuredBuffer<Particle> gParts : register(t1); // 루트 SRV(param2)로 바인드 — 입력레이아웃/VB 불필요
cbuffer BillCB : register(b1) { float3 gCamRight; float _p0; float3 gCamUp; float _p1; };
struct VOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float3 col:COLOR; float soft:TEXCOORD1; };
VOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    float2 cs[6] = { float2(-1,-1), float2(1,-1), float2(1,1), float2(-1,-1), float2(1,1), float2(-1,1) };
    float2 c = cs[vid];
    Particle p = gParts[iid];
    float3 wp = p.pos + gCamRight * (c.x * p.size) + gCamUp * (c.y * p.size);
    VOut o; o.pos = mul(float4(wp, 1.0), gMVP); o.uv = c; o.col = p.col; o.soft = p.soft; return o;
}
float4 PSMain(VOut i) : SV_TARGET
{
    float r = saturate(1.0 - dot(i.uv, i.uv));  // 원형 폴오프
    r = pow(r, max(i.soft, 0.05));              // soft<1=넓은 글로우(연기), >1=뾰족(스파크)
    return float4(i.col * r, r);                // 프리멀티플라이드 (가산/알파 PSO 공용)
}
