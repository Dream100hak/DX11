// SsaoNormalDepth.hlsl
// SSAO 입력용 normal-depth 패스.
//   출력 = float4( view-space normal.xyz , view-space depth(z) )
// FX 00. SsaoNormalDepth.fx (모델 오버라이드) 의 HLSL 대체.
// 본/스키닝/트윈 로직은 Standard_VS.hlsl 과 동일 (자기완결 복제 — 기존 셰이더 관례 유지).

#include "Common.hlsli"

#define MAX_MODEL_TRANSFORMS 500
#define MAX_MODEL_INSTANCE   500

// ── Bone / Tween CB (Standard_VS 와 동일 레이아웃) ──
cbuffer BoneBuffer : register(b4)
{
    matrix BoneTransforms[MAX_MODEL_TRANSFORMS];
};

cbuffer ModelBoneBuffer : register(b5)
{
    matrix MeshBoneTransform;   // 정적 모델: 메시별 본 변환
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
Texture2D      DiffuseMap   : register(t0);

// ── Vertex Input ──
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
    float4 position     : POSITION;
    float2 uv           : TEXCOORD;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
    uint   instanceID   : SV_InstanceID;
    matrix world        : INST_WORLD;
    uint   isPicked     : PICKED;
};

// ── Output : view-space normal + view-space position ──
struct NDOutput
{
    float4 position  : SV_POSITION;
    float2 uv        : TEXCOORD0;
    float3 viewNormal: NORMAL;
    float3 viewPos   : POSITION;
};

// ── Animation skin matrix (Standard_VS 와 동일) ──
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

// view-space normal 계산 헬퍼 (world normal -> view)
float3 ToViewNormal(float3 normalW)
{
    return normalize(mul(normalW, (float3x3)V));
}

// ── VS_Mesh ──
NDOutput VS_Mesh(VertexMesh input)
{
    NDOutput output;
    float4 posW      = mul(float4(input.position.xyz, 1.0f), input.world);
    output.viewPos   = mul(posW, V).xyz;
    output.position  = mul(posW, VP);
    output.uv        = input.uv;
    float3 normalW   = mul(input.normal, (float3x3)input.world);
    output.viewNormal= ToViewNormal(normalW);
    return output;
}

// ── VS_Model (정적 모델, 메시별 본) ──
NDOutput VS_Model(VertexModel input)
{
    NDOutput output;
    float4 localPos  = mul(float4(input.position.xyz, 1.0f), MeshBoneTransform);
    float4 posW      = mul(localPos, input.world);
    output.viewPos   = mul(posW, V).xyz;
    output.position  = mul(posW, VP);
    output.uv        = input.uv;
    float3 normalW   = mul(input.normal, (float3x3)input.world);
    output.viewNormal= ToViewNormal(normalW);
    return output;
}

// ── VS_Animation (스키닝 + 트윈) ──
NDOutput VS_Animation(VertexModel input)
{
    NDOutput output;
    matrix skinMat   = GetAnimationMatrix(input);
    float4 skinnedPos= mul(float4(input.position.xyz, 1.0f), skinMat);
    float4 posW      = mul(skinnedPos, input.world);
    output.viewPos   = mul(posW, V).xyz;
    output.position  = mul(posW, VP);
    output.uv        = input.uv;
    float3 normalW   = mul(input.normal, (float3x3)input.world);
    output.viewNormal= ToViewNormal(normalW);
    return output;
}

// ── PS : view-space normal(xyz) + view-space depth(z) ──
float4 PS_Main(NDOutput input) : SV_Target
{
    return float4(normalize(input.viewNormal), input.viewPos.z);
}
