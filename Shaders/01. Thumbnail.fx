#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

float4 PS(MeshOutput input) : SV_TARGET
{
    float3 lightDirection = normalize(float3(0.0, -1.0, 0.0)); // 상단에서 아래로
    float3 lightColor = float3(0.3f, 0.3f, 0.3f); // 흰색 광원
    float lightIntensity = 1.0;

    float3 normal = normalize(input.normal);

    float ndotl = max(dot(normal, lightDirection), 0.0);
    float3 diffuse = ndotl * lightColor * lightIntensity;

    float3 ambientColor = float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambientColor + diffuse;

    return float4(finalColor, 1.0);
}


float4 PS_TEX(MeshOutput input) : SV_TARGET
{
    float3 lightDirection = normalize(float3(0.0, -1.0, 0.0)); // 상단에서 아래로
    float3 lightColor = float3(0.3f, 0.3f, 0.3f); // 흰색 광원
    float lightIntensity = 1.0;

    float3 normal = normalize(input.normal);

    float ndotl = max(dot(normal, lightDirection), 0.0);
    float3 diffuse = ndotl * lightColor * lightIntensity;

    float3 ambientColor = float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambientColor + diffuse;

    if (Material.useTexture)
    {
    //    float4 texColor = DiffuseMap.Sample(LinearSampler, input.uv);
     //   finalColor *= texColor.rgb;
    }

    // 와이어프레임 색상 혼합
    float3 wireframeColor = float3(1.0, 1.0, 1.0); // 흰색 와이어프레임
    float3 combinedColor = lerp(finalColor, wireframeColor, 0.5);

    return float4(combinedColor, 1.0);
}

technique11 T0
{
    PASS_VP(P0, VS_Mesh , PS)
    PASS_VP(P1, VS_Model, PS)
    PASS_VP(P2, VS_Animation, PS)
    PASS_RS_VP(P3, FillModeWireFrame, VS_Mesh, PS_TEX)
    PASS_RS_VP(P4, FillModeWireFrame, VS_Model, PS_TEX)
    PASS_RS_VP(P5, FillModeWireFrame, VS_Animation, PS_TEX)
};
