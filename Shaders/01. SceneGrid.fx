#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

VertexColorOutput VS(VertexColor input)
{
    VertexColorOutput output;
    
    output.position = mul(input.position, W);
    output.position = mul(output.position, VP);
    output.color = input.color;

    return output;
}

float4 PS(VertexColorOutput input) : SV_TARGET
{
    return input.color;
}

technique11 T0
{
    PASS_VP(P0, VS, PS)
};
