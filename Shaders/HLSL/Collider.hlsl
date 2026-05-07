// Collider.hlsl
// 콜라이더 디버그 라인 렌더 (GS 포함)

#include "Common.hlsli" // VertexColor 는 Common.hlsli 에서 정의됨

struct ColorOutput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

// ── VS ───────────────────────────────────────────────────────
ColorOutput VS_Main(VertexColor input)
{
    ColorOutput output;
    output.position = mul(input.position, W);
    output.position = mul(output.position, VP);
 output.color    = input.color;
    return output;
}

// ── GS (라인 패스스루) ────────────────────────────────────────
[maxvertexcount(2)]
void GS_Main(line ColorOutput input[2],
           inout LineStream<ColorOutput> lineStream)
{
    lineStream.Append(input[0]);
    lineStream.Append(input[1]);
}

// ── PS ───────────────────────────────────────────────────────
float4 PS_Main(ColorOutput input) : SV_TARGET
{
    return input.color;
}
