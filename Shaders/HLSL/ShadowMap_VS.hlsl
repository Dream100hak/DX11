// ShadowMap_VS.hlsl
// Depth-only pass (Shadow Map 생성)
// PS 없음 ? 깊이만 기록

#include "Common.hlsli"

// ── Bone / Tween (Standard_VS 와 동일 레이아웃) ──────────────
#define MAX_MODEL_TRANSFORMS 500
#define MAX_MODEL_INSTANCE   500

cbuffer BoneBuffer : register(b4)
{
    matrix BoneTransforms[MAX_MODEL_TRANSFORMS];
};

struct KeyframeDesc
{
    int   animIndex;
    uint  currFrame;
    uint  nextFrame;
    float ratio;
    float sumTime;
 float speed;
    float2 padding;
};

struct TweenFrameDesc
{
    float tweenDuration;
    float tweenRatio;
    float tweenSumTime;
    float padding;
    KeyframeDesc curr;
    KeyframeDesc next;
};

cbuffer TweenBuffer : register(b6)
{
    TweenFrameDesc TweenFrames[MAX_MODEL_INSTANCE];
};

Texture2DArray TransformMap : register(t5);

// ── 라이트 공간 변환용 CB (b7) ───────────────────────────────
cbuffer ShadowPassBuffer : register(b7)
{
    matrix LightVP;   // 라이트 View * Proj
    float  DepthBias;
    float  SlopeScaledBias;
    float2 ShadowPad;
};

// ── Vertex Input ─────────────────────────────────────────────
struct VertexMesh
{
    float4 position  : POSITION;
    float2 uv        : TEXCOORD;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    uint   instanceID: SV_InstanceID;
    matrix world     : INST_WORLD;
    uint   isPicked  : PICKED;
};

struct VertexModel
{
    float4 position : POSITION;
  float2 uv         : TEXCOORD;
    float3 normal     : NORMAL;
    float3 tangent      : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
    uint   instanceID   : SV_InstanceID;
    matrix world        : INST_WORLD;
    uint isPicked     : PICKED;
};

// ── Depth-only Output ────────────────────────────────────────
struct DepthOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;   // 알파클립용
};

// ── Animation Matrix (Standard_VS 와 동일) ───────────────────
matrix GetAnimationMatrix(VertexModel input)
{
    float indices[4] = { input.blendIndices.x, input.blendIndices.y,
     input.blendIndices.z, input.blendIndices.w };
    float weights[4] = { input.blendWeights.x, input.blendWeights.y,
           input.blendWeights.z, input.blendWeights.w };

    int   animIndex[2];
    int   currFrame[2];
    int   nextFrame[2];
    float ratio[2];

    animIndex[0] = TweenFrames[input.instanceID].curr.animIndex;
    currFrame[0] = TweenFrames[input.instanceID].curr.currFrame;
    nextFrame[0] = TweenFrames[input.instanceID].curr.nextFrame;
    ratio[0]     = TweenFrames[input.instanceID].curr.ratio;

    animIndex[1] = TweenFrames[input.instanceID].next.animIndex;
    currFrame[1] = TweenFrames[input.instanceID].next.currFrame;
    nextFrame[1] = TweenFrames[input.instanceID].next.nextFrame;
    ratio[1]     = TweenFrames[input.instanceID].next.ratio;

    float4 c0, c1, c2, c3, n0, n1, n2, n3;
    matrix currMat = 0, nextMat = 0, transform = 0;

    for (int i = 0; i < 4; i++)
 {
        c0 = TransformMap.Load(int4(indices[i]*4+0, currFrame[0], animIndex[0], 0));
      c1 = TransformMap.Load(int4(indices[i]*4+1, currFrame[0], animIndex[0], 0));
    c2 = TransformMap.Load(int4(indices[i]*4+2, currFrame[0], animIndex[0], 0));
     c3 = TransformMap.Load(int4(indices[i]*4+3, currFrame[0], animIndex[0], 0));
 currMat = matrix(c0, c1, c2, c3);

        n0 = TransformMap.Load(int4(indices[i]*4+0, nextFrame[0], animIndex[0], 0));
     n1 = TransformMap.Load(int4(indices[i]*4+1, nextFrame[0], animIndex[0], 0));
        n2 = TransformMap.Load(int4(indices[i]*4+2, nextFrame[0], animIndex[0], 0));
n3 = TransformMap.Load(int4(indices[i]*4+3, nextFrame[0], animIndex[0], 0));
        nextMat = matrix(n0, n1, n2, n3);

        matrix result = lerp(currMat, nextMat, ratio[0]);

  if (animIndex[1] >= 0)
        {
 c0 = TransformMap.Load(int4(indices[i]*4+0, currFrame[1], animIndex[1], 0));
            c1 = TransformMap.Load(int4(indices[i]*4+1, currFrame[1], animIndex[1], 0));
            c2 = TransformMap.Load(int4(indices[i]*4+2, currFrame[1], animIndex[1], 0));
      c3 = TransformMap.Load(int4(indices[i]*4+3, currFrame[1], animIndex[1], 0));
       currMat = matrix(c0, c1, c2, c3);

            n0 = TransformMap.Load(int4(indices[i]*4+0, nextFrame[1], animIndex[1], 0));
      n1 = TransformMap.Load(int4(indices[i]*4+1, nextFrame[1], animIndex[1], 0));
            n2 = TransformMap.Load(int4(indices[i]*4+2, nextFrame[1], animIndex[1], 0));
n3 = TransformMap.Load(int4(indices[i]*4+3, nextFrame[1], animIndex[1], 0));
       nextMat = matrix(n0, n1, n2, n3);

        matrix nextResult = lerp(currMat, nextMat, ratio[1]);
  result = lerp(result, nextResult, TweenFrames[input.instanceID].tweenRatio);
        }
        transform += mul(weights[i], result);
    }
    return transform;
}

// ── VS_Mesh ──────────────────────────────────────────────────
DepthOutput VS_Mesh(VertexMesh input)
{
    DepthOutput output;
    float4 worldPos  = mul(input.position, input.world);
    output.position  = mul(worldPos, LightVP);
    output.uv        = input.uv;
    return output;
}

// ── VS_Model ─────────────────────────────────────────────────
DepthOutput VS_Model(VertexModel input)
{
    DepthOutput output;
    float4 pos      = mul(input.position, BoneTransforms[0]);
    float4 worldPos = mul(pos, input.world);
    output.position = mul(worldPos, LightVP);
    output.uv       = input.uv;
    return output;
}

// ── VS_Animation ─────────────────────────────────────────────
DepthOutput VS_Animation(VertexModel input)
{
    DepthOutput output;
    matrix skinMat  = GetAnimationMatrix(input);
    float4 pos   = mul(input.position, skinMat);
    float4 worldPos = mul(pos, input.world);
    output.position = mul(worldPos, LightVP);
    output.uv       = input.uv;
 return output;
}
