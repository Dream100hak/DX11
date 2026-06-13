// PassViewer.hlsl
// 씬뷰 패스 뷰어 — GBuffer/SSAO/Shadow 등 렌더 패스를 풀스크린으로 시각화
// (에디터 씬 뷰 좌상단 콤보로 모드 선택, Camera::Render_Deferred 마지막에 그려짐)
//
// t0 : GBuffer Albedo (rgb=linear albedo, a=metallic)
// t1 : GBuffer Normal (xyz=packed normal, w=roughness)
// t2 : GBuffer Position (xyz=world pos, w=mask)
// t3 : SsaoMap
// t4 : ShadowMap (depth)

#include "Common.hlsli"

Texture2D GBufferAlbedo   : register(t0);
Texture2D GBufferNormal   : register(t1);
Texture2D GBufferPosition : register(t2);
Texture2D      SsaoMap    : register(t3);
Texture2DArray ShadowMap  : register(t4); // CSM 배열 — 디버그는 캐스케이드 0 표시

// C++ PassViewerDesc 와 일치
cbuffer PassViewerBuffer : register(b8)
{
    int    ViewMode;   // 1=Albedo 2=Normal 3=Roughness 4=Metallic 5=WorldPos 6=Depth 7=SSAO 8=Shadow
    float3 ViewerPad;
};

struct ViewerVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

ViewerVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    ViewerVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PS_Main(ViewerVSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 albedo   = GBufferAlbedo.Sample(PointSampler, uv);
    float4 normal   = GBufferNormal.Sample(PointSampler, uv);
    float4 position = GBufferPosition.Sample(PointSampler, uv);
    bool   hasGeom  = position.w >= 0.5f;

    float3 color = float3(0, 0, 0);

    if (ViewMode == 1)        // Albedo (linear → 감마 인코딩해서 표시)
    {
        color = pow(abs(albedo.rgb), 1.0f / 2.2f);
    }
    else if (ViewMode == 2)   // World Normal
    {
        color = hasGeom ? normal.xyz : float3(0.5f, 0.5f, 1.0f);
    }
    else if (ViewMode == 3)   // Roughness
    {
        color = hasGeom ? normal.www : float3(0, 0, 0);
    }
    else if (ViewMode == 4)   // Metallic
    {
        color = hasGeom ? albedo.aaa : float3(0, 0, 0);
    }
    else if (ViewMode == 5)   // World Position (반복 패턴)
    {
        color = hasGeom ? frac(position.xyz * 0.05f) : float3(0, 0, 0);
    }
    else if (ViewMode == 6)   // Depth (카메라 거리, near=white)
    {
        if (hasGeom)
        {
            float dist = length(position.xyz - CameraPositionWS());
            color = (1.0f - saturate(dist / 500.0f)).xxx;
        }
    }
    else if (ViewMode == 7)   // SSAO
    {
        color = SsaoMap.Sample(LinearSampler, uv).rrr;
    }
    else if (ViewMode == 8)   // Shadow Map (라이트 시점 depth)
    {
        float d = ShadowMap.Sample(PointSampler, float3(uv, 0)).r; // 캐스케이드 0 슬라이스
        // depth 분포가 far 쪽에 몰리므로 대비 강조
        color = pow(abs(d), 30.0f).xxx;
    }

    return float4(color, 1.0f);
}
