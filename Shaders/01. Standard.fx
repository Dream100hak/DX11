#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

float4 PS_Default(MeshOutput input) : SV_TARGET
{

    input.normal = normalize(input.normal);

    float3 toEye = normalize(CameraPosition() - input.worldPosition);
    // Cache the distance to the eye from this surface point.
    float distToEye = length(toEye);
    
    toEye /= distToEye;

    // Default to multiplicative identity.
    float4 texColor = Material.diffuse;
    if (Material.useTexture)
    {
        texColor = DiffuseMap.Sample(LinearSampler, input.uv);
      
        if (Material.useAlphaclip)
        {
            clip(texColor.a - 0.1f);
        }
    }

    //
    // Lighting.
    //

    float4 litColor = texColor;
    if (Material.lightCount > 0)
    {
        // Start with a sum of zero. 
        float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

        // Only the first light casts a shadow.
        float3 shadow = float3(1.0f, 1.0f, 1.0f);
        shadow[0] = CalcShadowFactor(ShadowSampler, ShadowMap, input.shadow);

        
        // Finish texture projection and sample SSAO map.
        input.ssao /= input.ssao.w;
        float ambientAccess = SsaoMap.SampleLevel(LinearSampler, input.ssao.xy, 0.0f).r;
        
        // Sum the light contribution from each light source.  
        [unroll]
        for (int i = 0; i < Material.lightCount; ++i)
        {
            float4 A, D, S;
            ComputeDirectionalLight(input.normal, toEye, A, D, S);

            ambient += ambientAccess * A;
            diffuse += shadow[i] * D;
            spec += shadow[i] * S;
        }

        litColor = texColor * (ambient + diffuse) + spec;
    }

    litColor.a = Material.diffuse.a * texColor.a;

    return litColor;
}


technique11 T0
{
	PASS_VP_TEXTURE(P0, VS_Mesh, PS_Default)
	PASS_VP_TEXTURE(P1, VS_Model, PS_Default)
	PASS_VP_TEXTURE(P2, VS_Animation, PS_Default)
};
technique11 T1
{
    PASS_VP(P0, VS_Mesh, PS_Default)
	PASS_VP(P1, VS_Model, PS_Default)
	PASS_VP(P2, VS_Animation, PS_Default)
};
technique11 T2
{
	PASS_VP_COLOR(P0, VS_Mesh, PS_Default)
	PASS_VP_COLOR(P1, VS_Model, PS_Default)
	PASS_VP_COLOR(P2, VS_Animation, PS_Default)
};

technique11 T3
{
	PASS_RS_VP(P0, FillModeWireFrame, VS_Mesh, PS_Default)
	PASS_RS_VP(P1, FillModeWireFrame, VS_Model, PS_Default)
	PASS_RS_VP(P2, FillModeWireFrame, VS_Animation, PS_Default)
};

technique11 T4
{
	PASS_RS_VP(P0, FrontCounterClockwiseTrue, VS_Mesh, PS_Default)
	PASS_RS_VP(P1, FrontCounterClockwiseTrue, VS_Model, PS_Default)
	PASS_RS_VP(P2, FrontCounterClockwiseTrue, VS_Animation, PS_Default)
};