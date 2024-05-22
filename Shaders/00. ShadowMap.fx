#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

cbuffer TerrainBuffer
{
	// When distance is minimum, the tessellation is maximum.
	// When distance is maximum, the tessellation is minimum.
    float MinDist;
    float MaxDist;

	// Exponents for power of 2 tessellation.  The tessellation
	// range is [2^(gMinTess), 2^(gMaxTess)].  Since the maximum
	// tessellation is 64, this means gMaxTess can be at most 6
	// since 2^6 = 64.
    float MinTess;
    float MaxTess;
    float2 TexScale;
};

Texture2D HeightMap;

struct VertexOut
{
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
    float2 BoundsY : TEXCOORD1;
};

struct HullOut
{
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
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
    float3 center = 0.25f * (patch[0].PosW + patch[1].PosW + patch[2].PosW + patch[3].PosW);
    float tessFactor = CalcTessFactor(center); // Simplified tessellation factor calculation
    pt.EdgeTess[0] = pt.EdgeTess[1] = pt.EdgeTess[2] = pt.EdgeTess[3] = tessFactor;
    pt.InsideTess[0] = pt.InsideTess[1] = tessFactor;
    return pt;
}

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POS;
    float2 Tex : TEXCOORD;
    float2 TiledTex : TEXCOORD1;
    float4 Shadow : TEXCOORD2;
};

// Domain Shader for shadow mapping
[domain("quad")]
DomainOut DS(PatchTess patchTess,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;

	// Bilinear interpolation.
    dout.PosW = lerp(
		lerp(quad[0].PosW, quad[1].PosW, uv.x),
		lerp(quad[2].PosW, quad[3].PosW, uv.x),
		uv.y);

    dout.Tex = lerp(
		lerp(quad[0].Tex, quad[1].Tex, uv.x),
		lerp(quad[2].Tex, quad[3].Tex, uv.x),
		uv.y);

	// Tile layer textures over terrain.
    dout.TiledTex = dout.Tex * TexScale;

	// Displacement mapping
    dout.PosW.y = HeightMap.SampleLevel(HeightmapSampler, dout.Tex, 0).r;

	// NOTE: We tried computing the normal in the shader using finite difference, 
	// but the vertices move continuously with fractional_even which creates
	// noticable light shimmering artifacts as the normal changes.  Therefore,
	// we moved the calculation to the pixel shader.  

	// Project to homogeneous clip space.
    dout.PosH = mul(float4(dout.PosW, 1.0f), VP);
    
    dout.Shadow = mul(float4(dout.PosW, 1.0f), Shadow);
    
    return dout;
}

void PS(VertexOut pin)
{
    float4 diffuse = DiffuseMap.Sample(LinearSampler, pin.Tex);

	// Don't write transparent pixels to the shadow map.
    clip(diffuse.a - 0.15f);
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void TessPS(DomainOut pin)
{
    float4 diffuse = DiffuseMap.Sample(LinearSampler, pin.Tex);

	// Don't write transparent pixels to the shadow map.
    clip(diffuse.a - 0.15f);
}


RasterizerState Depth
{
	// [From MSDN]
	// If the depth buffer currently bound to the output-merger stage has a UNORM format or
	// no depth buffer is bound the bias value is calculated like this: 
	//
	// Bias = (float)DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope;
	//
	// where r is the minimum representable value > 0 in the depth-buffer format converted to float32.
	// [/End MSDN]
	// 
	// For a 24-bit depth buffer, r = 1 / 2^24.
	//
	// Example: DepthBias = 100000 ==> Actual DepthBias = 100000/2^24 = .006

	// You need to experiment with these values for your scene.
    DepthBias = 100000;
    DepthBiasClamp = 0.0f;
    SlopeScaledDepthBias = 1.0f;
};

technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Mesh()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);

        SetRasterizerState(Depth);
    }

    pass P1
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Model()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);

        SetRasterizerState(Depth);
    }
};

technique11 T1
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Terrain()));
        SetHullShader(CompileShader(hs_5_0, HS()));
        SetDomainShader(CompileShader(ds_5_0, DS()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL); // No pixel shader needed

        SetRasterizerState(Depth);
    }
};
