#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

struct VertexGridOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION1;
    float2 uv : TEXCOORD;
};

VertexGridOutput VS(VertexTexture input)
{
    VertexGridOutput output;
    
    output.position = mul(input.position, W);
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;

    return output;
}

float4 PS(VertexGridOutput input) : SV_TARGET
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
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));

        SetRasterizerState(FillModeWireFrame);
      //  SetDepthStencilState(DisableDepth, 0);
    }
};
