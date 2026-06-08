// Standard_VS.hlsl
// MeshRenderer / ModelRenderer / ModelAnimator �� ���� ���ؽ� ��������
// Common.hlsli CB ���� ���:
//   b0: GlobalBuffer  b1: TransformBuffer  b4: BoneBuffer  b6: TweenBuffer

#include "Common.hlsli"

// ===========================================================
// Bone / Animation Constant Buffers  (b4, b5, b6)
// ===========================================================

#define MAX_MODEL_TRANSFORMS 500
#define MAX_MODEL_INSTANCE   500

cbuffer BoneBuffer : register(b4)
{
matrix BoneTransforms[MAX_MODEL_TRANSFORMS];
};

// ModelRenderer(정적 모델): 메시별 본 변환 (per-mesh rigid bind).
// FX 의 GetScalar("BoneIndex")->SetInt 대체 — 메시마다 본 행렬을 직접 push.
cbuffer ModelBoneBuffer : register(b5)
{
    matrix MeshBoneTransform;
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

Texture2DArray TransformMap : register(t5); // SRV ���� 5 (Material SRV 0~4 ����)

// ===========================================================
// Vertex Input Structures
// ===========================================================

// ---- MeshRenderer ----
struct VertexMesh
{
float4 position     : POSITION;
    float2 uv     : TEXCOORD;
    float3 normal    : NORMAL;
    float3 tangent      : TANGENT;
    // Instancing (InputSlot 1)
    uint   instanceID   : SV_InstanceID;
    matrix world        : INST_WORLD;
    uint   isPicked     : PICKED;
};

// ---- ModelRenderer / ModelAnimator ----
struct VertexModel
{
    float4 position      : POSITION;
    float2 uv            : TEXCOORD;
    float3 normal      : NORMAL;
    float3 tangent       : TANGENT;
    float4 blendIndices  : BLEND_INDICES;
    float4 blendWeights  : BLEND_WEIGHTS;
    // Instancing (InputSlot 1)
  uint   instanceID    : SV_InstanceID;
    matrix world  : INST_WORLD;
    uint   isPicked    : PICKED;
};

// ===========================================================
// GetAnimationMatrix  (Tween ������ ����)
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
    ratio[1]   = TweenFrames[input.instanceID].next.ratio;

    float4 c0, c1, c2, c3;
    float4 n0, n1, n2, n3;
    matrix currMat  = 0;
  matrix nextMat  = 0;
    matrix transform = 0;

    for (int i = 0; i < 4; i++)
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

        // Ʈ��(���� �ִϸ��̼�) ������
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
// VS_Mesh  ? MeshRenderer ��
// ===========================================================
MeshOutput VS_Mesh(VertexMesh input)
{
    MeshOutput output;

    // w=0 ����: InputLayout�� float3(R32G32B32_FLOAT)���� ���ε��ǹǷ� w=1 ����
    float4 posW = mul(float4(input.position.xyz, 1.0f), input.world);
    output.worldPosition = posW.xyz;

    // Shadow ��ǥ
    output.shadow = mul(posW, Shadow);

  // View / Proj
    output.position = mul(posW, VP);

    output.uv      = input.uv;
    output.normal  = normalize(mul(input.normal,  (float3x3)input.world));
    output.tangent = normalize(mul(input.tangent, (float3x3)input.world));

    // SSAO UV
    output.ssao    = mul(output.position, T);

    output.picked  = input.isPicked;

    return output;
}

// ===========================================================
// VS_Model  ? ModelRenderer (��Ű�� ����, �� �ε��� ����)
// ===========================================================
MeshOutput VS_Model(VertexModel input)
{
    MeshOutput output;

    // 메시별 본 변환 적용 (per-mesh rigid bind), 이후 인스턴스 월드
    float4 localPos = mul(float4(input.position.xyz, 1.0f), MeshBoneTransform);
    float4 posW     = mul(localPos, input.world);
    output.worldPosition = posW.xyz;

    output.shadow = mul(posW, Shadow);
    output.position = mul(posW, VP);

    output.uv      = input.uv;
    output.normal  = normalize(mul(input.normal,  (float3x3)input.world));
 output.tangent = normalize(mul(input.tangent, (float3x3)input.world));

    output.ssao    = mul(output.position, T);
    output.picked  = input.isPicked;

    return output;
}

// ===========================================================
// VS_Animation  ? ModelAnimator (��Ű�� + Tween ������)
// ===========================================================
MeshOutput VS_Animation(VertexModel input)
{
    MeshOutput output;

    matrix skinMat   = GetAnimationMatrix(input);

    float4 skinnedPos = mul(float4(input.position.xyz, 1.0f), skinMat);
    float4 posW       = mul(skinnedPos, input.world);
    output.worldPosition = posW.xyz;

    output.shadow    = mul(posW, Shadow);
    output.position  = mul(posW, VP);

    output.uv        = input.uv;
    output.normal    = normalize(mul(input.normal,  (float3x3)input.world));
    output.tangent   = normalize(mul(input.tangent, (float3x3)input.world));

    output.ssao  = mul(output.position, T);
    output.picked    = input.isPicked;

    return output;
}
