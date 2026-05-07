// Terrain.hlsl
// Tessellation 지형 : VS → HS → DS → PS
// CB 슬롯: b0(Global) b2(Light) b3(Material) b8(Terrain)

#include "Lighting.hlsli"
#include "Shadow.hlsli"

// ── Terrain 전용 CB ───────────────────────────────────────────
cbuffer TerrainBuffer : register(b8)
{
    float FogStart;
    float FogRange;
    float4 FogColor;

    float MinDist;
    float MaxDist;
    float MinTess;
    float MaxTess;

    float TexelCellSpaceU;
    float TexelCellSpaceV;
    float WorldCellSpace;
    float2 TexScale;
    float2 TerrainPad;

    float4 WorldFrustumPlanes[6];
};

// ── Textures ──────────────────────────────────────────────────
Texture2DArray LayerMapArray : register(t0);
Texture2D      BlendMap      : register(t1);
Texture2D      HeightMap     : register(t2);
Texture2D    ShadowMap     : register(t3);

// ── Vertex Input ──────────────────────────────────────────────
struct VertexTerrain
{
    float3 PosL  : POS;
    float2 Tex     : TEXCOORD;
    float2 BoundsY : TEXCOORD1;
    float3 Normal  : NORMAL;
};

// ── VS Output (HS Input) ──────────────────────────────────────
struct VertexOut
{
    float3 PosW    : POS;
    float2 Tex     : TEXCOORD;
    float2 BoundsY : TEXCOORD1;
    float3 Normal  : NORMAL;
};

// ── Frustum Culling ───────────────────────────────────────────
bool AabbBehindPlane(float3 center, float3 extents, float4 plane)
{
float r = dot(extents, abs(plane.xyz));
    float s = dot(float4(center, 1.f), plane);
    return (s + r) < 0.f;
}

bool AabbOutsideFrustum(float3 center, float3 extents, float4 planes[6])
{
    for (int i = 0; i < 6; i++)
      if (AabbBehindPlane(center, extents, planes[i])) return true;
    return false;
}

// ── VS ───────────────────────────────────────────────────────
VertexOut VS_Main(VertexTerrain vin)
{
    VertexOut vout;
    vout.PosW    = vin.PosL;
    vout.PosW.y  = HeightMap.SampleLevel(HeightmapSampler, vin.Tex, 0).r;
    vout.Tex     = vin.Tex;
    vout.BoundsY = vin.BoundsY;
    vout.Normal  = vin.Normal;
  return vout;
}

// ── Tess Factor ───────────────────────────────────────────────
float CalcTessFactor(float3 p)
{
    float d = distance(p, CameraPositionWS());
    float s = saturate((d - MinDist) / (MaxDist - MinDist));
    return pow(2.f, lerp(MaxTess, MinTess, s));
}

// ── HS ───────────────────────────────────────────────────────
struct PatchTess
{
    float EdgeTess[4]   : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;

    // Frustum Culling
    float  minY = patch[0].BoundsY.x, maxY = patch[0].BoundsY.y;
    float3 vMin = float3(patch[2].PosW.x, minY, patch[2].PosW.z);
    float3 vMax = float3(patch[1].PosW.x, maxY, patch[1].PosW.z);
    float3 boxCenter  = 0.5f * (vMin + vMax);
    float3 boxExtents = 0.5f * (vMax - vMin);

    if (AabbOutsideFrustum(boxCenter, boxExtents, WorldFrustumPlanes))
    {
        pt.EdgeTess[0] = pt.EdgeTess[1] = pt.EdgeTess[2] = pt.EdgeTess[3] = 0;
        pt.InsideTess[0] = pt.InsideTess[1] = 0;
        return pt;
    }

    float3 e0 = 0.5f * (patch[0].PosW + patch[2].PosW);
    float3 e1 = 0.5f * (patch[0].PosW + patch[1].PosW);
    float3 e2 = 0.5f * (patch[1].PosW + patch[3].PosW);
    float3 e3 = 0.5f * (patch[2].PosW + patch[3].PosW);
    float3 c= 0.25f * (patch[0].PosW + patch[1].PosW + patch[2].PosW + patch[3].PosW);

    pt.EdgeTess[0] = CalcTessFactor(e0);
    pt.EdgeTess[1] = CalcTessFactor(e1);
  pt.EdgeTess[2] = CalcTessFactor(e2);
    pt.EdgeTess[3] = CalcTessFactor(e3);
    pt.InsideTess[0] = CalcTessFactor(c);
    pt.InsideTess[1] = pt.InsideTess[0];
    return pt;
}

struct HullOut
{
    float3 PosW : POS;
    float2 Tex  : TEXCOORD;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS_Main(InputPatch<VertexOut, 4> p,
        uint i : SV_OutputControlPointID,
       uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosW = p[i].PosW;
    hout.Tex  = p[i].Tex;
    return hout;
}

// ── DS Output (PS Input) ─────────────────────────────────────
struct DomainOut
{
    float4 PosH     : SV_POSITION;
float3 PosW   : POSITION;
    float2 Tex      : TEXCOORD0;
    float2 TiledTex : TEXCOORD1;
    float4 Shadow   : TEXCOORD2;
};

[domain("quad")]
DomainOut DS_Main(PatchTess patchTess,
      float2 uv : SV_DomainLocation,
    const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
    dout.PosW = lerp(lerp(quad[0].PosW, quad[1].PosW, uv.x),
lerp(quad[2].PosW, quad[3].PosW, uv.x), uv.y);
  dout.Tex= lerp(lerp(quad[0].Tex,  quad[1].Tex,  uv.x),
   lerp(quad[2].Tex,  quad[3].Tex,  uv.x), uv.y);
    dout.TiledTex = dout.Tex * TexScale;

    // 높이맵 displacement
    dout.PosW.y  = HeightMap.SampleLevel(HeightmapSampler, dout.Tex, 0).r;
  dout.Shadow  = CalcShadowCoord(dout.PosW);
    dout.PosH    = mul(float4(dout.PosW, 1.f), VP);
    return dout;
}

// ── PS ───────────────────────────────────────────────────────
float4 PS_Main(DomainOut pin) : SV_TARGET
{
    // 중앙차분으로 법선 계산
    float2 leftTex   = pin.Tex + float2(-TexelCellSpaceU, 0.f);
    float2 rightTex  = pin.Tex + float2( TexelCellSpaceU, 0.f);
    float2 bottomTex = pin.Tex + float2(0.f,  TexelCellSpaceV);
    float2 topTex    = pin.Tex + float2(0.f, -TexelCellSpaceV);

    float leftY = HeightMap.SampleLevel(HeightmapSampler, leftTex,   0).r;
    float rightY  = HeightMap.SampleLevel(HeightmapSampler, rightTex,  0).r;
    float bottomY = HeightMap.SampleLevel(HeightmapSampler, bottomTex, 0).r;
    float topY    = HeightMap.SampleLevel(HeightmapSampler, topTex,    0).r;

    float3 tangent = normalize(float3(2.f * WorldCellSpace, rightY - leftY, 0.f));
    float3 bitan   = normalize(float3(0.f, bottomY - topY, -2.f * WorldCellSpace));
    float3 normalW = cross(tangent, bitan);

    float3 toEye = normalize(CameraPositionWS() - pin.PosW);

    // 레이어 텍스처 블렌딩
    float4 c0 = LayerMapArray.Sample(LinearSampler, float3(pin.TiledTex, 0.f));
  float4 c1 = LayerMapArray.Sample(LinearSampler, float3(pin.TiledTex, 1.f));
    float4 c2 = LayerMapArray.Sample(LinearSampler, float3(pin.TiledTex, 2.f));
    float4 c3 = LayerMapArray.Sample(LinearSampler, float3(pin.TiledTex, 3.f));
    float4 c4 = LayerMapArray.Sample(LinearSampler, float3(pin.TiledTex, 4.f));
    float4 t  = BlendMap.Sample(LinearSampler, pin.Tex);

    float4 texColor = c0;
    texColor = lerp(texColor, c1, t.r);
    texColor = lerp(texColor, c2, t.g);
    texColor = lerp(texColor, c3, t.b);
    texColor = lerp(texColor, c4, t.a);

    // 라이팅
  float shadowFactor = CalcShadowFactor(ShadowMap, pin.Shadow);

    float4 A, D, S;
    ComputeDirectionalLight(normalW, toEye, A, D, S);

    float4 litColor = texColor * (A + shadowFactor * D) + shadowFactor * S;
    litColor.a = 1.f;
    return litColor;
}
