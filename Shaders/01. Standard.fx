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

            if (Material.useSsao)
                ambient += ambientAccess * A;
            else
                ambient += A;
            
            diffuse += shadow[i] * D;
            spec += shadow[i] * S;
        }

        litColor = texColor * (ambient + diffuse) + spec;
    }

    litColor.a = Material.diffuse.a * texColor.a;

    return litColor;
}

float4 PS_TEST(MeshOutput input) : SV_TARGET
{
     // 카메라 위치의 월드 좌표를 가져옵니다.
    float3 cameraPos = CameraPosition();

    // 그리드의 월드 좌표와 카메라의 월드 좌표를 이용하여 거리를 계산합니다.
    float distanceToCamera = distance(input.worldPosition, cameraPos);

    // 그리드 정점 간의 거리를 UV 좌표를 기준으로 비교합니다.
    float maxDistance = 30.0f; // 최대 거리, 이 거리 이상일 경우 완전히 투명
    float fadeStartDistance = 15.0f; // 페이드가 시작되는 거리

    // 페이드 시작 거리와 최대 거리 사이에서 알파 값을 선형적으로 감소시킵니다.
    float alpha = saturate((maxDistance - distanceToCamera) / (maxDistance - fadeStartDistance));

    float4 color = float4(1.f, 1.f, 1.f, 0.5f);
    color.a *= alpha;

    // 알파 테스트: 알파 값이 일정 이하이면 픽셀을 버립니다.
    if (color.a <= 0.15f)
    {
        discard;
    }

    return color;
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
	PASS_RS_VP(P0, FillModeWireFrame, VS_Mesh, PS_TEST)
	PASS_RS_VP(P1, FillModeWireFrame, VS_Model, PS_Default)
	PASS_RS_VP(P2, FillModeWireFrame, VS_Animation, PS_Default)
};

technique11 T4
{
	PASS_RS_VP(P0, FrontCounterClockwiseTrue, VS_Mesh, PS_Default)
	PASS_RS_VP(P1, FrontCounterClockwiseTrue, VS_Model, PS_Default)
	PASS_RS_VP(P2, FrontCounterClockwiseTrue, VS_Animation, PS_Default)
};