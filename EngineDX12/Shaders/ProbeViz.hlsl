// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

struct PSH { float3 c0; float3 c1; float3 c2; float3 c3; };
StructuredBuffer<PSH> gProbes : register(t1);
struct VOut { float4 pos:SV_POSITION; float3 col:COLOR; };
VOut VSMain(uint id : SV_VertexID)
{
    VOut o;
    uint PX = (uint)gGridDim.x, PY = (uint)gGridDim.y, PZ = (uint)gGridDim.z;
    uint px = id % PX, py = (id / PX) % PY, pz = id / (PX * PY);
    float3 t = float3(px / max(PX - 1.0, 1.0), py / max(PY - 1.0, 1.0), pz / max(PZ - 1.0, 1.0));
    float3 wp = lerp(gGridMin.xyz, gGridMax.xyz, t);
    o.pos = mul(float4(wp, 1.0), gMVP);
    o.col = max(gProbes[id].c0 * 0.886, 0.03);
    return o;
}
float4 PSMain(VOut i) : SV_TARGET { return float4(i.col, 1.0); }
