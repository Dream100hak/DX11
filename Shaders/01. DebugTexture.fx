#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"


float4 PS(MeshOutput input) : SV_TARGET
{
    float4 c = DiffuseMap.Sample(LinearSampler, input.uv);
    return float4(c.rrr, 1);
}

technique11 T0
{
    PASS_VP(P0 , VS_Mesh , PS)
    PASS_VP(P1, VS_Model, PS)
    PASS_VP(P2, VS_Animation, PS)
};
