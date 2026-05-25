// GBufferDebug.hlsl
// G-Buffer debug visualization - 4-quadrant split view
//
// Top-Left     : Final lit result (passthrough from backbuffer - shows albedo as fallback)
// Top-Right    : World Normals (color-coded)
// Bottom-Left  : World Position (normalized by distance)
// Bottom-Right : Depth (grayscale, near=white far=black)

#include "Common.hlsli"

Texture2D GBufferAlbedo   : register(t0);
Texture2D GBufferNormal   : register(t1);
Texture2D GBufferPosition : register(t2);

struct DebugVSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

DebugVSOutput VS_Main(uint vertexID : SV_VertexID)
{
    DebugVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PS_Main(DebugVSOutput input) : SV_TARGET
{
    float2 uv = input.uv;

    // quadrant: 0=TL, 1=TR, 2=BL, 3=BR
    int col = (uv.x < 0.5f) ? 0 : 1;
    int row = (uv.y < 0.5f) ? 0 : 1;
    int quadrant = row * 2 + col;

    float2 localUV = frac(uv * 2.0f);

    // border
    float2 edge = abs(frac(uv * 2.0f) - 0.5f) * 2.0f;
    float borderMask = (edge.x > 0.99f || edge.y > 0.99f) ? 1.0f : 0.0f;
    if (borderMask > 0.5f)
        return float4(0.3f, 0.3f, 0.3f, 1.0f);

    float4 result = float4(0, 0, 0, 1);

    if (quadrant == 0)
    {
        // Albedo
        result = GBufferAlbedo.Sample(PointSampler, localUV);
        result.a = 1.0f;
    }
    else if (quadrant == 1)
    {
        // World Normal (packed [0,1] in G-Buffer, show as color)
        float4 n = GBufferNormal.Sample(PointSampler, localUV);
        result = float4(n.xyz, 1.0f);
    }
    else if (quadrant == 2)
    {
        // World Position (visualize by frac for repeating pattern)
        float4 pos = GBufferPosition.Sample(PointSampler, localUV);
        if (pos.w < 0.5f)
            result = float4(0.05f, 0.05f, 0.05f, 1.0f);
        else
            result = float4(frac(pos.xyz * 0.1f), 1.0f);
    }
    else
    {
        // Depth (from camera, grayscale)
        float4 pos = GBufferPosition.Sample(PointSampler, localUV);
        if (pos.w < 0.5f)
        {
            result = float4(0, 0, 0, 1);
        }
        else
        {
            float3 camPos = CameraPositionWS();
            float dist = length(pos.xyz - camPos);
            float normalizedDepth = saturate(dist / 500.0f);
            float gray = 1.0f - normalizedDepth;
            result = float4(gray, gray, gray, 1.0f);
        }
    }

    return result;
}
