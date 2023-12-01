#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

MeshOutput VS_MeshOutline(VertexMesh input)
{
    MeshOutput output;
    
    input.position.xyz += input.normal * 0.1f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.
    
    output.position = mul(input.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = input.normal;
    
    return output;
}

MeshOutput VS_ModelOutline(VertexModel input)
{
    MeshOutput output;
    
    input.position.xyz += input.normal * 0.05f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.

    output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
    output.position = mul(output.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;

    output.normal = input.normal;
   
    return output;
}

MeshOutput VS_AnimationOutline(VertexModel input)
{
    MeshOutput output;

    input.position.xyz += input.normal * 0.1f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.

    matrix m = GetAnimationMatrix(input);

    output.position = mul(input.position, m);
    output.position = mul(output.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = mul(input.normal, (float3x3) input.world);
    output.tangent = mul(input.tangent, (float3x3) input.world);

    return output;
}

float4 PS(MeshOutput input) : SV_TARGET  
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

technique11 T0
{
	PASS_VP(P0, VS_MeshOutline, PS)
	PASS_VP(P1, VS_ModelOutline, PS)
	PASS_VP(P2, VS_AnimationOutline, PS)
};
