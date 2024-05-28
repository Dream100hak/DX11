#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

cbuffer TerrainBuffer
{
    float MinDist;
    float MaxDist;

    float MinTess;
    float MaxTess;
};

Texture2D HeightMap;

struct VertexOut
{
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
    float2 BoundsY : TEXCOORD1;
    float3 Normal : NORMAL;
};

struct HullOut
{
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
    float3 Normal : NORMAL;
};

VertexOut VS_Terrain(VertexTerrain vin)
{
    VertexOut vout;

	// Terrain specified directly in world space.
    vout.PosW = vin.PosL;

	// Displace the patch corners to world space.  This is to make 
	// the eye to patch distance calculation more accurate.
    vout.PosW.y = HeightMap.SampleLevel(HeightmapSampler, vin.Tex, 0).r;

	// Output vertex attributes to next stage.
    vout.Tex = vin.Tex;
    vout.BoundsY = vin.BoundsY;
    vout.Normal = vin.Normal;
    
    return vout;
}

float CalcTessFactor(float3 p)
{
    float3 viewDir = -float3(V._31, V._32, V._33);
    float d = dot(normalize(viewDir), normalize(GlobalLight.direction));
    float s = saturate(1.0 - abs(d));

    return pow(2, (lerp(MaxTess, MinTess, s)));
}


[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
    HullOut hout;

	// Pass through shader.
    hout.PosW = p[i].PosW;
    hout.Tex = p[i].Tex;
    hout.Normal = p[i].Normal;
    
    return hout;
}

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};


PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
   
    // Compute midpoint on edges, and patch center
    float3 e0 = 0.5f * (patch[0].PosW + patch[2].PosW);
    float3 e1 = 0.5f * (patch[0].PosW + patch[1].PosW);
    float3 e2 = 0.5f * (patch[1].PosW + patch[3].PosW);
    float3 e3 = 0.5f * (patch[2].PosW + patch[3].PosW);
    float3 c = 0.25f * (patch[0].PosW + patch[1].PosW + patch[2].PosW + patch[3].PosW);

    pt.EdgeTess[0] = CalcTessFactor(e0);
    pt.EdgeTess[1] = CalcTessFactor(e1);
    pt.EdgeTess[2] = CalcTessFactor(e2);
    pt.EdgeTess[3] = CalcTessFactor(e3);

    pt.InsideTess[0] = CalcTessFactor(c);
    pt.InsideTess[1] = pt.InsideTess[0];

    return pt;
}

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
    float3 Normal : NORMAL;
    float3 WorldViewPosition : POS1;
};

// Domain Shader for shadow mapping
[domain("quad")]
DomainOut DS(PatchTess patchTess,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;

    dout.PosW = lerp(
		lerp(quad[0].PosW, quad[1].PosW, uv.x),
		lerp(quad[2].PosW, quad[3].PosW, uv.x),
		uv.y);

    dout.Tex = lerp(
		lerp(quad[0].Tex, quad[1].Tex, uv.x),
		lerp(quad[2].Tex, quad[3].Tex, uv.x),
		uv.y);

	// Displacement mapping
    dout.PosW.y = HeightMap.SampleLevel(HeightmapSampler, dout.Tex, 0).r;
    
    dout.WorldViewPosition = mul(float4(dout.PosW, 1.0f), V);
    
    dout.Normal = lerp(
        lerp(quad[0].Normal, quad[1].Normal, uv.x),
        lerp(quad[2].Normal, quad[3].Normal, uv.x),
        uv.y);
    
    dout.PosH = mul(float4(dout.PosW, 1.0f), VP);
    return dout;
}

float4 PS(MeshOutput input, uniform bool alphaClip) : SV_Target
{
    input.normal = normalize(input.normal);

    if (alphaClip)
    {
        float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);	 
        clip(texColor.a - 0.1f);
    }
	
    return float4(input.normal, input.worldViewPosition.z);
}

float4 PS_Terrain(DomainOut input, uniform bool alphaClip) : SV_Target
{
    input.Normal = normalize(input.Normal);
    
    return float4(input.Normal, input.WorldViewPosition.z);
}

technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Mesh()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
    pass P1
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Model()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
    pass P2
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Animation()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
}

technique11 T1
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Terrain()));
        SetHullShader(CompileShader(hs_5_0, HS()));
        SetDomainShader(CompileShader(ds_5_0, DS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS_Terrain(false)));
    }
};
