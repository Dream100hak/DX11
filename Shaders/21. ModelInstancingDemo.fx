#include "00. Global.fx"
#include "00. Light.fx"

#define MAX_MODEL_TRANSFORMS 250

cbuffer BoneBuffer
{
	matrix BoneTransforms[MAX_MODEL_TRANSFORMS];
};

uint BoneIndex;

struct VS_IN
{
	float4 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float4 blendIndices : BLEND_INDICES;
	float4 blendWeights : BLEND_WEIGHTS;
	// INSTANCING;
	uint instanceID : SV_INSTANCEID;
	matrix world : INST;
};

struct VS_OUT
{
	float4 position : SV_POSITION;
	float3 worldPosition : POSITION1;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

VS_OUT VS(VS_IN input)
{
	VS_OUT output;

	output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
	output.position = mul(output.position, input.world); // W
	output.worldPosition = output.position;
	output.position = mul(output.position, VP);
	output.uv = input.uv;
	output.normal = input.normal;

	return output;
}

float4 PS(VS_OUT input) : SV_TARGET
{
	//float4 color = ComputeLight(input.normal, input.uv, input.worldPosition);

	float4 color = DiffuseMap.Sample(LinearSampler, input.uv);

	return color;
}

technique11 T0
{
	PASS_VP(P0, VS, PS)
};
