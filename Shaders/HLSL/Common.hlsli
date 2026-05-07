// Common.hlsli
// HlslShader 기반 CB 슬롯 레이아웃 및 공통 버텍스 구조체 정의
// 슬롯 약속:
//   b0 : GlobalBuffer
//   b1 : TransformBuffer
//   b2 : LightBuffer
//   b3 : MaterialBuffer
//   b4 : BoneBuffer
//   b5 : KeyframeBuffer
//   b6 : TweenBuffer
//
// Sampler 슬롯 (RenderStateManager::BindAllSamplersPS 순서):
//   s0 : LinearSampler     (MIN_MAG_MIP_LINEAR WRAP)
//   s1 : PointSampler(MIN_MAG_MIP_POINT  WRAP)
//   s2 : AnisotropicSampler
//   s3 : ShadowSampler     (Comparison LESS, BORDER)
//   s4 : HeightmapSampler  (MIN_MAG_LINEAR_MIP_POINT CLAMP)

#ifndef _COMMON_HLSLI_
#define _COMMON_HLSLI_

// ===========================================================
// Constant Buffers
// ===========================================================

cbuffer GlobalBuffer : register(b0)
{
    matrix V;
 matrix P;
    matrix VP;
    matrix VInv;
    matrix Shadow;
    matrix T;   // UV 텍스처 변환 (SSAO 등)
};

cbuffer TransformBuffer : register(b1)
{
 matrix W;
};

cbuffer LightBuffer : register(b2)
{
    float4 LightAmbient;
    float4 LightDiffuse;
    float4 LightSpecular;
    float4 LightEmissive;
    float3 LightDirection;
    float  LightIntensity;
};

cbuffer MaterialBuffer : register(b3)
{
    float4 MatAmbient;
    float4 MatDiffuse;
    float4 MatSpecular;
    float4 MatEmissive;
 int    UseTexture;
int    UseAlphaClip;
    int    UseSsao;
    int    LightCount;
};

// BoneBuffer (b4), KeyframeBuffer (b5), TweenBuffer (b6) 은
// Standard_VS.hlsl 등 애니메이션 셰이더에서 별도 선언

// ===========================================================
// Samplers
// ===========================================================

SamplerState           LinearSampler  : register(s0);
SamplerState           PointSampler       : register(s1);
SamplerState        AnisotropicSampler : register(s2);
SamplerComparisonState ShadowSampler      : register(s3);
SamplerState        HeightmapSampler   : register(s4);

// ===========================================================
// Vertex Structures
// ===========================================================

struct Vertex
{
    float4 position : POSITION;
};

struct VertexTexture
{
    float4 position : POSITION;
    float2 uv     : TEXCOORD;
};

struct VertexColor
{
    float4 position : POSITION;
    float4 color    : COLOR;
};

struct VertexTextureNormal
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
};

struct VertexTextureNormalTangent
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct VertexTextureNormalTangentBlend
{
    float4 position      : POSITION;
    float2 uv  : TEXCOORD;
    float3 normal        : NORMAL;
    float3 tangent       : TANGENT;
    float4 blendIndices  : BLEND_INDICES;
    float4 blendWeights  : BLEND_WEIGHTS;
};

// ===========================================================
// Instancing 추가 슬롯 (InputSlot 1)
// ===========================================================
struct InstancingData
{
    matrix   instWorld    : INST_WORLD;
 uint instPicked   : PICKED;
};

// ===========================================================
// Pixel Shader Input (공통 출력)
// ===========================================================
struct MeshOutput
{
    float4 position      : SV_POSITION;
    float2 uv            : TEXCOORD0;
    float3 normal        : NORMAL;
    float3 tangent       : TANGENT;
    float3 worldPosition : TEXCOORD1;
    float4 shadow      : TEXCOORD2;   // shadow map 좌표
    float4 ssao          : TEXCOORD3;   // SSAO 텍스처 좌표
    uint   picked        : TEXCOORD4;
};

// ===========================================================
// Utility
// ===========================================================
float3 CameraPositionWS()
{
    // VInv 의 41_42_43 이 카메라 월드 위치
    return float3(VInv._41, VInv._42, VInv._43);
}

#endif // _COMMON_HLSLI_
