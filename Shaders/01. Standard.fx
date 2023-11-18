#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

float4 PS(MeshOutput input, 
            uniform int lightCount,
            uniform bool useTexture,
            uniform bool alphaClip) : SV_TARGET
{

    //float4 light = ComputeLight(input.normal, input.uv, input.worldPosition);
    //float color = DiffuseMap.Sample(LinearSampler, input.uv);
    
    //return color * light;
   
     // Interpolating normal can unnormalize it, so normalize it.
    input.normal = normalize(input.normal);

    // The toEye vector is used in lighting.
    float3 toEye = normalize(CameraPosition() - input.worldPosition);
    // Cache the distance to the eye from this surface point.
    float distToEye = length(toEye);
    
    toEye /= distToEye;

    // Default to multiplicative identity.
    float4 texColor = float4(1, 1, 1, 1);
    if (useTexture)
    {
        // Sample texture.
        texColor = DiffuseMap.Sample(LinearSampler, input.uv);

        if (alphaClip)
        {
            // Discard pixel if texture alpha < 0.1.  Note that we do this
            // test as soon as possible so that we can potentially exit the shader 
            // early, thereby skipping the rest of the shader code.
            clip(texColor.a - 0.1f);
        }
    }

    //
    // Lighting.
    //

    float4 litColor = texColor;
    if (lightCount > 0)
    {
        // Start with a sum of zero. 
        float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

        // Only the first light casts a shadow.
        float3 shadow = float3(1.0f, 1.0f, 1.0f);
        shadow[0] = CalcShadowFactor(ShadowSampler, ShadowMap, input.shadow);

        // Sum the light contribution from each light source.  
        [unroll]
        for (int i = 0; i < lightCount; ++i)
        {
            float4 A, D, S;
            ComputeDirectionalLight(input.normal, toEye, A, D, S);

            ambient += A;
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
	PASS_VP_TEXTURE(P0, VS_Mesh, PS)
	PASS_VP_TEXTURE(P1, VS_Model, PS)
	PASS_VP_TEXTURE(P2, VS_Animation, PS)
};
