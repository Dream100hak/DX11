#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

float4x4 WVP;

VertexOutput VS(VertexTexture input)
{
    VertexOutput vout;
    
    vout.position = mul(float4(input.position.xyz, 1.0f), WVP);
    vout.uv = input.uv;

    return vout;
}

float4 PS_Albedo(VertexOutput input) : SV_Target
{
    return DiffuseMap.Sample(LinearSampler, input.uv);
}
float4 PS_Red(VertexOutput input, uniform int index) : SV_TARGET
{
    float4 c = DiffuseMap.Sample(LinearSampler, input.uv);
  //  return c;
    // draw as grayscale
    return float4(c.rrr, 1);
}

technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS_Albedo()));
    }
}

technique11 T1
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS_Red(0)));
    }
}