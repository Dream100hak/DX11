#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

float4 PS(MeshOutput input, uniform bool gAlphaClip) : SV_Target
{
    input.normal = normalize(input.normal);

    if (gAlphaClip)
    {
        float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);
		 
        clip(texColor.a - 0.1f);
    }
	
    return float4(input.normal, input.worldViewPosition.z);
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
        SetVertexShader(CompileShader(vs_5_0, VS_Model()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
    pass P2
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Animation()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
}
