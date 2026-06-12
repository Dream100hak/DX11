// Outline_VS.hlsl
// 선택 오브젝트 아웃라인 — 스텐실 2패스용 VS (단일 드로우 전용, 인스턴싱 없음)
// Pass 1(마크) : OutlineWidth = 0, 컬러 기록 없이 스텐실만 채움
// Pass 2(팽창) : 노멀 방향 팽창 + 단색 PS (Outline_PS)
// 월드 행렬은 b1 TransformBuffer.W 사용 (Camera::RenderOutlinePass 에서 push)

#include "Common.hlsli"

#define MAX_MODEL_INSTANCE 500

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

// ModelRenderer 정적 모델: 메시당 1개 본 변환 (메시마다 b5 push)
cbuffer ModelBoneBuffer : register(b5)
{
    matrix MeshBoneTransform;
};

Texture2DArray TransformMap : register(t5);

cbuffer OutlineBuffer : register(b8)
{
    float4 OutlineColor;  // 아웃라인 색상
    float  OutlineWidth;  // 팽창 크기 (월드 단위)
    float3 OutlinePad;
};

// ===========================================================
// Vertex Input
// ===========================================================
struct VertexMesh
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct VertexModel
{
    float4 position     : POSITION;
    float2 uv           : TEXCOORD;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
    uint   instanceID   : SV_InstanceID; // 단일 드로우 = 0 (TweenFrames[0])
};

struct OutlineOutput
{
    float4 position : SV_POSITION;
};

// ===========================================================
// GetAnimationMatrix (GBufferAnim 과 동일한 트윈 스키닝)
// ===========================================================
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

    for (int i = 0; i < 4; ++i)
    {
        c0 = TransformMap.Load(int4(indices[i] * 4 + 0, currFrame[0], animIndex[0], 0));
        c1 = TransformMap.Load(int4(indices[i] * 4 + 1, currFrame[0], animIndex[0], 0));
        c2 = TransformMap.Load(int4(indices[i] * 4 + 2, currFrame[0], animIndex[0], 0));
        c3 = TransformMap.Load(int4(indices[i] * 4 + 3, currFrame[0], animIndex[0], 0));
        currMat = matrix(c0, c1, c2, c3);

        n0 = TransformMap.Load(int4(indices[i] * 4 + 0, nextFrame[0], animIndex[0], 0));
        n1 = TransformMap.Load(int4(indices[i] * 4 + 1, nextFrame[0], animIndex[0], 0));
        n2 = TransformMap.Load(int4(indices[i] * 4 + 2, nextFrame[0], animIndex[0], 0));
        n3 = TransformMap.Load(int4(indices[i] * 4 + 3, nextFrame[0], animIndex[0], 0));
        nextMat = matrix(n0, n1, n2, n3);

        matrix result = lerp(currMat, nextMat, ratio[0]);

        if (animIndex[1] >= 0)
        {
            c0 = TransformMap.Load(int4(indices[i] * 4 + 0, currFrame[1], animIndex[1], 0));
            c1 = TransformMap.Load(int4(indices[i] * 4 + 1, currFrame[1], animIndex[1], 0));
            c2 = TransformMap.Load(int4(indices[i] * 4 + 2, currFrame[1], animIndex[1], 0));
            c3 = TransformMap.Load(int4(indices[i] * 4 + 3, currFrame[1], animIndex[1], 0));
            currMat = matrix(c0, c1, c2, c3);

            n0 = TransformMap.Load(int4(indices[i] * 4 + 0, nextFrame[1], animIndex[1], 0));
            n1 = TransformMap.Load(int4(indices[i] * 4 + 1, nextFrame[1], animIndex[1], 0));
            n2 = TransformMap.Load(int4(indices[i] * 4 + 2, nextFrame[1], animIndex[1], 0));
            n3 = TransformMap.Load(int4(indices[i] * 4 + 3, nextFrame[1], animIndex[1], 0));
            nextMat = matrix(n0, n1, n2, n3);

            matrix nextResult = lerp(currMat, nextMat, ratio[1]);
            result = lerp(result, nextResult, TweenFrames[input.instanceID].tweenRatio);
        }

        transform += mul(weights[i], result);
    }

    return transform;
}

// ===========================================================
// VS_MeshOutline — MeshRenderer (정적 메시)
// ===========================================================
OutlineOutput VS_MeshOutline(VertexMesh input)
{
    OutlineOutput output;

    input.position.xyz += input.normal * OutlineWidth;

    float4 worldPos  = mul(input.position, W);
    output.position  = mul(worldPos, VP);

    return output;
}

// ===========================================================
// VS_ModelOutline — ModelRenderer (정적 모델, 메시별 본 변환 b5)
// ===========================================================
OutlineOutput VS_ModelOutline(VertexModel input)
{
    OutlineOutput output;

    input.position.xyz += input.normal * OutlineWidth;

    float4 pos      = mul(input.position, MeshBoneTransform);
    float4 worldPos = mul(pos, W);
    output.position = mul(worldPos, VP);

    return output;
}

// ===========================================================
// VS_AnimationOutline — ModelAnimator (트윈 스키닝)
// ===========================================================
OutlineOutput VS_AnimationOutline(VertexModel input)
{
    OutlineOutput output;

    input.position.xyz += input.normal * OutlineWidth;

    matrix skinMat  = GetAnimationMatrix(input);
    float4 pos      = mul(input.position, skinMat);
    float4 worldPos = mul(pos, W);
    output.position = mul(worldPos, VP);

    return output;
}
