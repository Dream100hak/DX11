#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

// texture can use a NULL pixel shader for depth pass.
void PS(MeshOutput pin)
{
    float4 diffuse = DiffuseMap.Sample(LinearSampler, pin.uv);

	// Don't write transparent pixels to the shadow map.
    clip(diffuse.a - 0.15f);
}


RasterizerState Depth
{
	// [From MSDN]
	// If the depth buffer currently bound to the output-merger stage has a UNORM format or
	// no depth buffer is bound the bias value is calculated like this: 
	//
	// Bias = (float)DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope;
	//
	// where r is the minimum representable value > 0 in the depth-buffer format converted to float32.
	// [/End MSDN]
	// 
	// For a 24-bit depth buffer, r = 1 / 2^24.
	//
	// Example: DepthBias = 100000 ==> Actual DepthBias = 100000/2^24 = .006

	// You need to experiment with these values for your scene.
    DepthBias = 100000;
    DepthBiasClamp = 0.0f;
    SlopeScaledDepthBias = 1.0f;
};

technique11 T0
{
	PASS_RS_VP(P0, Depth, VS_Mesh, PS)
};
