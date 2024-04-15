//float gRadius = 0.001f;
//float gFar = 10000.f;
//float gFalloff = 0.000002f;
//float gStrength = 0.0007f;
//float gTotStrength = 1.38f; 
//float gInvSamples = 1.f / 16.f;

//float3 gRandom[16] =
//{
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),
//    float3(0.202f, 0.641f, -0.906f),  
//};

//struct SsaoIn
//{
   
//    float2 uv : TEXCOORD;
//    float depth : DEPTH;
//    float viewZ : VIEWZ;
//    float3 Normal : NORMAL;
//};

//float3 RandomNormal(float2 uv)
//{
//    float noiseX = (frac(sin(dot(uv, float2(15.8989f, 76.132f) * 1.0f)) * 46336.2345f));
//    float noiseY = (frac(sin(dot(uv, float2(15.8989f, 76.132f) * 1.0f)) * 46336.2345f));
//    float noiseZ = (frac(sin(dot(uv, float2(15.8989f, 76.132f) * 1.0f)) * 46336.2345f));
    
//    return normalize(float3(noiseX, noiseY, noiseZ));
//}


//// Nonnumeric values cannot be added to a cbuffer.
//Texture2D gNormalDepthMap;
//sampler DepthSam = sampler_state
//{
//    Texture = gNormalDepthMap;
//};

//struct SsaoOut
//{
//    float2 uv : TEXCOORD;
//    float4 ambient : COLOR;
//};

//SsaoOut VS(SsaoIn vin)
//{
//    SsaoOut vout = (SsaoOut) 0.f; 
    
//    half3 vRay;
//    half3 vReflect;
//    half2 vRandomUV;
//    float fOccNorm;
	
//    int iColor = 0; 
    
//    for (int i = 0; i < 16; i++)
//    {
//        vRay = reflect(RandomNormal(vin.uv), gRandom[i]);
//        vReflect = normalize(reflect(vRay, vin.Normal)) * gRadius;
//        vReflect.x *= -1.f;
//        vRandomUV = vin.uv + vReflect.xy;
//        //fOccNorm = tex2D(DepthSam, vRandomUV).g * gFar * vin.viewZ;
        
//        //if(fOccNorm <= vin.depth + 0.0003f)
//          //  ++iColor;

//    }
    
//    vout.ambient =  abs((iColor / 16.f) - 1);
    
//    return vout;
//}


//float4 PS(SsaoOut pin, uniform int gSampleCount) : SV_Target
//{
//    float4 vDepth = tex2D(DepthSam, pin.uv);
//    //float4 vDepth = tex2D(DepthSam, pin.uv);
    
//    return float4(1, 1, 1, 1);

//}

//technique11 T0
//{
//    pass P0
//    {
//        SetVertexShader(CompileShader(vs_5_0, VS()));
//        SetPixelShader(CompileShader(ps_5_0, PS(14)));
//    }
//}
 