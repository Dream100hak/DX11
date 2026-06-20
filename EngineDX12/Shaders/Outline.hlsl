// 자동 분리: D3D12Shaders.cpp 인라인 문자열 → .hlsl 파일 (런타임 DXC 컴파일, #include 로 공용 SceneCB)
#include "SceneCB.hlsli"

struct VIn { float3 pos:POSITION; float3 nrm:NORMAL; float3 col:COLOR; float2 uv:TEXCOORD; float3 tan:TANGENT; };
float4 VSMain(VIn i) : SV_POSITION
{
    float3 wp = i.pos;                 // 정점은 이미 월드(스키닝/기즈모 반영)
    float3 n = normalize(i.nrm);
    float d = distance(gCamPos.xyz, wp);
    wp += n * d * gOutline.w;          // 두께(카메라 거리 비례)
    return mul(float4(wp, 1.0), gMVP);
}
float4 PSMain() : SV_TARGET { return float4(gOutline.rgb, 1.0); } // 아웃라인 색(HDR)
