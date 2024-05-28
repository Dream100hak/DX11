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
     // ī�޶� ��ġ�� ���� ��ǥ�� �����ɴϴ�.
    float3 cameraPos = CameraPosition();

    // �׸����� ���� ��ǥ�� ī�޶��� ���� ��ǥ�� �̿��Ͽ� �Ÿ��� ����մϴ�.
    float distanceToCamera = distance(input.worldPosition, cameraPos);

    // �׸��� ���� ���� �Ÿ��� UV ��ǥ�� �������� ���մϴ�.
    float maxDistance = 30.0f; // �ִ� �Ÿ�, �� �Ÿ� �̻��� ��� ������ ����
    float fadeStartDistance = 15.0f; // ���̵尡 ���۵Ǵ� �Ÿ�

    // ���̵� ���� �Ÿ��� �ִ� �Ÿ� ���̿��� ���� ���� ���������� ���ҽ�ŵ�ϴ�.
    float alpha = saturate((maxDistance - distanceToCamera) / (maxDistance - fadeStartDistance));

    float4 color = float4(1.f, 1.f, 1.f, 0.5f);
    color.a *= alpha;

    // ���� �׽�Ʈ: ���� ���� ���� �����̸� �ȼ��� �����ϴ�.
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
