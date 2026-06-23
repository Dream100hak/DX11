// 속도 G버퍼 — 오브젝트 "고유" 화면속도(카메라 모션 제외)만 RG16F 로 기록.
// 카메라 모션은 모션블러 패스가 depth+prevVP 로 별도 복원해 합산(분해). 따라서 정적 메시는 0.
// 정점은 이미 월드 베이크(스키닝/리베이크) — gPrevVB[vid].pos = 직전 프레임 월드 위치.
#include "SceneCB.hlsli"

// 런타임 정점(Vtx) 레이아웃과 바이트 일치 (pos/nrm/col/uv/tan = 56B)
struct Vtx { float3 pos; float3 nrm; float3 col; float2 uv; float3 tan; };
StructuredBuffer<Vtx> gPrevVB : register(t0); // 루트 SRV(slot1) — 직전 프레임 월드 정점

struct VIn  { float3 pos:POSITION; float3 nrm:NORMAL; float3 col:COLOR; float2 uv:TEXCOORD; float3 tan:TANGENT; uint vid:SV_VertexID; };
struct VOut { float4 pos:SV_POSITION; float4 curC:TEXCOORD0; float4 prevC:TEXCOORD1; };

VOut VSMain(VIn i)
{
    VOut o;
    float3 curW  = i.pos;            // 현재 월드 (래스터용 — 지터 무관)
    float3 prevW = gPrevVB[i.vid].pos;
    o.pos   = mul(float4(curW, 1.0), gCurVPnj);   // 깊이 테스트 위치(현재 카메라)
    o.curC  = o.pos;                              // 현재 카메라 기준 현재 위치
    o.prevC = mul(float4(prevW, 1.0), gCurVPnj);  // 현재 카메라 기준 직전 위치 → 차이=오브젝트 고유 모션
    return o;
}

float2 PSMain(VOut i) : SV_TARGET
{
    float2 cur  = i.curC.xy  / i.curC.w;
    float2 prev = i.prevC.xy / i.prevC.w;
    return (cur - prev) * float2(0.5, -0.5); // NDC차 → UV 속도(Y 뒤집힘)
}
