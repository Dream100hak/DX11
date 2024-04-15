#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

struct MeshOutput02
{
    float4 position : SV_POSITION;
    float3 worldViewPosition : POSITION1;
    float2 uv : TEXCOORD;
    float3 normalV : NORMAL;
};


MeshOutput02 VS_Model02(VertexModel input)
{
    MeshOutput02 output;

    output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
    output.position = mul(output.position, input.world); // W
    output.worldViewPosition = mul(output.position, V);
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normalV = mul(input.normal, (float3x3) WInvTransposeV);
  //  output.normalV = input.normal;
    return output;
}

float4 PS(MeshOutput input, uniform bool gAlphaClip) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
  //  input.normal = mul(input.normal, (float3x3) WInvTransposeV);
    input.normal = normalize(input.normal);

    if (gAlphaClip)
    {
        float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);
		 
        clip(texColor.a - 0.1f);
    }
	
    return float4(input.normal, input.worldViewPosition.z);
}
float4 PS02(MeshOutput02 input, uniform bool gAlphaClip) : SV_Target
{

    input.normalV = normalize(input.normalV);

    if (gAlphaClip)
    {
        float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);
		 
        clip(texColor.a - 0.1f);
    }
	
    return float4(input.normalV, input.worldViewPosition.z);
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
        SetVertexShader(CompileShader(vs_5_0, VS_Model02()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS02(false)));
    }
    pass P2
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Animation()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
}
