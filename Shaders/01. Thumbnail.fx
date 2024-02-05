#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

struct VertexModelThumbnail
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;

};
struct MeshOutputThumbnail
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION1;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 shadow : TEXCOORD1;
};

MeshOutputThumbnail VS_ModelThumbnail(VertexModelThumbnail input)
{
    MeshOutput output;

    output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
    output.position = mul(output.position, W); // W
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;

    output.normal = input.normal;
		
	// Generate projective tex-coords to project shadow map onto scene.
    output.shadow = mul(float4(output.worldPosition.xyz, 1.0f), Shadow);
	
	
    return output;
}

float4 PS(MeshOutputThumbnail input) : SV_TARGET
{

    //// // Interpolating normal can unnormalize it, so normalize it.
    //input.normal = normalize(input.normal);

    //// The toEye vector is used in lighting.
    //float3 toEye = normalize(CameraPosition() - input.worldPosition);
    //// Cache the distance to the eye from this surface point.
    //float distToEye = length(toEye);
    
    //toEye /= distToEye;
    
    //float4 texColor = Material.diffuse;
    //texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    //float4 litColor = texColor;
    //int lightCount = 3;

    //    // Start with a sum of zero. 
    //float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    //float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    //float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);
 
    //[unroll]
    //for (int i = 0; i < lightCount; ++i)
    //{
    //    float4 A, D, S;
    //    ComputeDirectionalLight(input.normal, toEye, A, D, S);

    //    ambient += A;
    //    diffuse += D;
    //    spec += S;
    //}

    //litColor = texColor * (ambient + diffuse) + spec;
    //litColor.a = Material.diffuse.a * texColor.a;
    
    //float grayscale = (litColor.r + litColor.g + litColor.b) / 3.0; // RGB 값을 평균내어 그레이스케일 값 계산
    //return float4(grayscale, grayscale, grayscale, 1); // 그레이스케일 색상으로 반환
    
// 여기에 조명 계산 코드를 추가합니다.
    // 예시로, 간단한 방향성 광원을 사용합니다.

    // 광원 방향, 색상, 강도를 정의합니다.
    float3 lightDirection = normalize(float3(0.0, -1.0, 0.0)); // 상단에서 아래로
    float3 lightColor = float3(0.3f, 0.3f, 0.3f); // 흰색 광원
    float lightIntensity = 1.0;

    // 노멀 벡터를 정규화합니다.
    float3 normal = normalize(input.normal);

    // 라이팅 계산: Lambertian 반사를 사용하여 간단한 확산 반사를 계산합니다.
    float ndotl = max(dot(normal, lightDirection), 0.0);
    float3 diffuse = ndotl * lightColor * lightIntensity;

    // 최종 색상: 확산 반사 + 앰비언트 컬러
    // 앰비언트 컬러는 임의로 설정할 수 있습니다.
    float3 ambientColor = float3(0.5f, 0.5f, 0.5f);
    float3 finalColor = ambientColor + diffuse;

    return float4(finalColor, 1.0);

}

technique11 T0
{
    PASS_VP(P0 , VS_Mesh , PS)
    PASS_VP(P1, VS_ModelThumbnail, PS)
    PASS_VP(P2, VS_Animation, PS)
};
