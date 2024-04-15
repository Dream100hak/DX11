cbuffer SsaoBlurBuffer
{
    float TexelWidth;
    float TexelHeight;
    float2 Dummy;
};

cbuffer WeightSettings
{
    float Weights[11] =
    {
        0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f
    };
};

// Nonnumeric values cannot be added to a cbuffer.
Texture2D NormalDepthMap;
Texture2D InputImage;

SamplerState samNormalDepth
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

SamplerState samInputImage
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

struct VertexSSao
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOut VS(VertexSSao vin)
{
    VertexOut vout;
	
	// Already in NDC space.
    vout.PosH = vin.pos;

	// Pass onto pixel shader.
    vout.uv = vin.uv;
	
    return vout;
}


float4 PS(VertexOut pin, uniform bool gHorizontalBlur) : SV_Target
{
    float2 texOffset;
    if (gHorizontalBlur)
    {
        texOffset = float2(TexelWidth, 0.0f);
    }
    else
    {
        texOffset = float2(0.0f, TexelHeight);
    }
    
    int gBlurRadius = 5;

	// The center value always contributes to the sum.
    float4 color = Weights[5] * InputImage.SampleLevel(samInputImage, pin.uv, 0.0);
    float totalWeight = Weights[5];
	 
    float4 centerNormalDepth = NormalDepthMap.SampleLevel(samNormalDepth, pin.uv, 0.0f);
    
   // return centerNormalDepth;

    for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
		// We already added in the center weight.
        if (i == 0)
            continue;

        float2 tex = pin.uv + i * texOffset;

        float4 neighborNormalDepth = NormalDepthMap.SampleLevel(
			samNormalDepth, tex, 0.0f);

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
        if (dot(neighborNormalDepth.xyz, centerNormalDepth.xyz) >= 0.8f &&
		    abs(neighborNormalDepth.a - centerNormalDepth.a) <= 0.2f)
        {
            float weight = Weights[i + gBlurRadius];

			// Add neighbor pixel to blur.
            color += weight * InputImage.SampleLevel(
				samInputImage, tex, 0.0);
		
            totalWeight += weight;
        }
    }

	// Compensate for discarded samples by making total weights sum to 1.
    return color / totalWeight;
}

technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(true)));
    }
}

technique11 T1
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(false)));
    }
}
 