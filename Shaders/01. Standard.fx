#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

MeshOutput VS_MeshOutline(VertexMesh input)
{
    MeshOutput output;
    
    float outlineThickness = 0.4f;
    
    output.position = mul(input.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    
    if (input.isPicked == 1)
    {
        output.position.xy += outlineThickness * normalize(input.normal.xy);
        float depthOffset = 0.001; // 적당한 깊이 오프셋 값 설정
        output.position.z += depthOffset; // 깊이 오프셋 적용
    }

    
    output.uv = input.uv;
    output.normal = input.normal;
    
    return output;
}

MeshOutput VS_ModelOutline(VertexModel input)
{
   
    MeshOutput output;
    
    float outlineThickness = 0.02f;

    // 모델의 월드 공간에서 정점 위치 계산
    float4 worldPosition = mul(input.position, BoneTransforms[BoneIndex]); // Bone Transform
    worldPosition = mul(worldPosition, input.world); // Model's World Transform

    // 아웃라인을 적용할 경우 월드 공간에서 두께 조절
    if (input.isPicked == 1)
    {
        float3 normalWorld = normalize(mul(input.normal, (float3x3) input.world)); // World space normal
        worldPosition.xyz += normalWorld * outlineThickness;
    }

    // 최종 화면 좌표계로 변환
    output.position = mul(worldPosition, VP);

    // 기타 속성 설정
    output.uv = input.uv;
    output.normal = input.normal;

    return output;
}

MeshOutput VS_AnimationOutline(VertexModel input)
{
    MeshOutput output;
    
    float outlineThickness = 0.4f;

    matrix m = GetAnimationMatrix(input);

    output.position = mul(input.position, m);
    output.position = mul(output.position, input.world); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    
    if (input.isPicked == 1)
    {
        output.position.xy += outlineThickness * normalize(input.normal.xy);
    }

    
    output.uv = input.uv;
    output.normal = mul(input.normal, (float3x3) input.world);
    output.tangent = mul(input.tangent, (float3x3) input.world);

    return output;
}


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

float4 PS_Outline(MeshOutput input) : SV_TARGET
{
    return float4(1.0f, 0.271f, 0.0f, 0.5f);
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
	PASS_VP(P1 , VS_ModelOutline, PS_Outline)
	PASS_VP(P2, VS_AnimationOutline, PS_Outline)
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