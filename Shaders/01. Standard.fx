#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

MeshOutput VS_MeshOutline(VertexMesh input)
{
    MeshOutput output;
    
    input.position.xyz += input.normal * 0.1f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.
    
    output.position = mul(input.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = input.normal;
    
    return output;
}

MeshOutput VS_ModelOutline(VertexModel input)
{
    MeshOutput output;
    
    if(input.isPicked == 1)
    {
        input.position.xyz += input.normal * 0.01f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.
    }

    output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
    output.position = mul(output.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;

    output.normal = input.normal;
   
    return output;
}

MeshOutput VS_AnimationOutline(VertexModel input)
{
    MeshOutput output;

    input.position.xyz += input.normal * 0.1f; // 0.1f는 아웃라인의 두께를 조절하는 값입니다.

    matrix m = GetAnimationMatrix(input);

    output.position = mul(input.position, m);
    output.position = mul(output.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = mul(input.normal, (float3x3) input.world);
    output.tangent = mul(input.tangent, (float3x3) input.world);

    return output;
}


float4 PS_Default(MeshOutput input, 
            uniform int lightCount,
            uniform bool useTexture,
            uniform bool alphaClip) : SV_TARGET
{

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

float4 PS_Outline(MeshOutput input) : SV_TARGET
{
    return float4(1.0f, 0.271f, 0.0f, 1.0f);
}

technique11 T0
{
	PASS_VP_TEXTURE(P0, VS_Mesh, PS_Default)
	PASS_VP_TEXTURE(P1, VS_Model, PS_Default)
	PASS_VP_TEXTURE(P2, VS_Animation, PS_Default)
};
technique11 T1
{
	PASS_VP(P0, VS_MeshOutline, PS_Outline)
	PASS_VP(P1, VS_ModelOutline, PS_Outline)
	PASS_VP(P2, VS_AnimationOutline, PS_Outline)
};