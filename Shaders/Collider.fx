#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"


VertexColorOutput VS(VertexColor input)
{
    VertexColorOutput output;
    
    output.position = mul(input.Position, W);
    output.position = mul(output.position, VP);
    output.color = input.Color;

    return output;
}

[maxvertexcount(4)]
void GS(line VertexColorOutput input[2], inout LineStream<VertexColorOutput> lineStream)
{
    lineStream.Append(input[0]);
    lineStream.Append(input[1]);
}

float4 PS(VertexColorOutput input) : SV_TARGET
{
    return input.color;
}

technique11 T0
{
    PASS_LINE_VP(P0, VS, GS, PS)
};
