#include "00. Global.fx"
#include "00. Light.fx"

struct VertexIn
{
    float3 PosL : POSITION;
};
struct VS_OUT
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION1;
};

VS_OUT VS(VertexTextureNormalTangent vin)
{
    VS_OUT vout;
    vout.PosH = mul(float4(vin.position.xyz, 0.f), VP ).xyww;
    vout.PosL = vin.position;

    return vout;
}

float4 PS(VS_OUT pin) : SV_Target
{
    return CubeMap.Sample(LinearSampler, pin.PosL);
}

DepthStencilState LessEqualDSS
{
    DepthFunc = LESS_EQUAL;
};


technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS()));

        SetRasterizerState(NoCull);
        SetDepthStencilState(LessEqualDSS, 0);
    }
}
