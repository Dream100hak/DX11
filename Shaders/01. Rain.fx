#include "00. Global.fx"

//***********************************************
// GLOBALS                                      *
//***********************************************

cbuffer cbPerFrame
{
	// for when the emit position/direction is varying
    float3 EmitPosW;
    float3 EmitDirW;

    float GameTime;
    float TimeStep;
    //float4x4 VP;
};

cbuffer cbFixed
{
	// Net constant acceleration used to accerlate the particles.
    float3 gAccelW = { -1.0f, -9.8f, 0.0f };
};

// Array of textures for texturing the particles.
Texture2DArray TexArray;

// Random texture used to generate random numbers in shaders.
Texture1D RandomTex;

//***********************************************
// HELPER FUNCTIONS                             *
//***********************************************
float3 RandUnitVec3(float offset)
{
	// Use game time plus offset to sample random texture.
    float u = (GameTime + offset);

	// coordinates in [-1,1]
    float3 v = RandomTex.SampleLevel(LinearSampler, u, 0).xyz;

	// project onto unit sphere
    return normalize(v);
}

float3 RandVec3(float offset)
{
	// Use game time plus offset to sample random texture.
    float u = (GameTime + offset);

	// coordinates in [-1,1]
    float3 v = RandomTex.SampleLevel(LinearSampler, u, 0).xyz;

    return v;
}

//***********************************************
// STREAM-OUT TECH                              *
//***********************************************

#define PT_EMITTER 0
#define PT_RAIN 1

struct VertexParticle
{
    float3 InitialPosW : POS;
    float3 InitialVelW : VELOCITY;
    float2 SizeW : SIZE;
    float Age : AGE;
    uint Type : TYPE;
};

VertexParticle StreamOutVS(VertexParticle vin)
{
    return vin;
}

// The stream-out GS is just responsible for emitting 
// new particles and destroying old particles.  The logic
// programed here will generally vary from particle system
// to particle system, as the destroy/spawn rules will be 
// different.
[maxvertexcount(6)]
void StreamOutGS(point VertexParticle gin[1],
	inout PointStream<VertexParticle> ptStream)
{
    gin[0].Age += TimeStep;

    if (gin[0].Type == PT_EMITTER)
    {
		// time to emit a new particle?
        if (gin[0].Age > 0.002f)
        {
            for (int i = 0; i < 5; ++i)
            {
				// Spread rain drops out above the camera.
                float3 vRandom = 35.0f * RandVec3((float) i / 5.0f);
                vRandom.y = 20.0f;

                VertexParticle p;
                p.InitialPosW = EmitPosW.xyz + vRandom;
                p.InitialVelW = float3(0.0f, 0.0f, 0.0f);
                p.SizeW = float2(1.0f, 1.0f);
                p.Age = 0.0f;
                p.Type = PT_RAIN;

                ptStream.Append(p);
            }

			// reset the time to emit
            gin[0].Age = 0.0f;
        }

		// always keep emitters
        ptStream.Append(gin[0]);
    }
    else
    {
		// Specify conditions to keep particle; this may vary from system to system.
        if (gin[0].Age <= 3.0f)
            ptStream.Append(gin[0]);
    }
}

GeometryShader gsStreamOut = ConstructGSWithSO(
	CompileShader(gs_5_0, StreamOutGS()),
	"POS.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; TYPE.x");

technique11 T0
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, StreamOutVS()));
        SetGeometryShader(gsStreamOut);

		// disable pixel shader for stream-out only
        SetPixelShader(NULL);

		// we must also disable the depth buffer for stream-out only
        SetDepthStencilState(DisableDepth, 0);
    }
}

//***********************************************
// DRAW TECH                                    *
//***********************************************

struct VertexOut
{
    float3 PosW : POS;
    uint Type : TYPE;
};

VertexOut DrawVS(VertexParticle vin)
{
    VertexOut vout;

    float t = vin.Age;

	// constant acceleration equation
    vout.PosW = 0.5f * t * t * gAccelW + t * vin.InitialVelW + vin.InitialPosW;

    vout.Type = vin.Type;

    return vout;
}

struct GeoOut
{
    float4 PosH : SV_Position;
    float2 Tex : TEXCOORD;
};

// The draw GS just expands points into lines.
[maxvertexcount(2)]
void DrawGS(point VertexOut gin[1],
	inout LineStream<GeoOut> lineStream)
{
	// do not draw emitter particles.
    if (gin[0].Type != PT_EMITTER)
    {
		// Slant line in acceleration direction.
        float3 p0 = gin[0].PosW;
        float3 p1 = gin[0].PosW + 0.07f * gAccelW;

        GeoOut v0;
        v0.PosH = mul(float4(p0, 1.0f), VP);
        v0.Tex = float2(0.0f, 0.0f);
        lineStream.Append(v0);

        GeoOut v1;
        v1.PosH = mul(float4(p1, 1.0f), VP);
        v1.Tex = float2(1.0f, 1.0f);
        lineStream.Append(v1);
    }
}

float4 DrawPS(GeoOut pin) : SV_TARGET
{
    return TexArray.Sample(LinearSampler, float3(pin.Tex, 0));
}

technique11 T1
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, DrawVS()));
        SetGeometryShader(CompileShader(gs_5_0, DrawGS()));
        SetPixelShader(CompileShader(ps_5_0, DrawPS()));

        SetDepthStencilState(NoDepthWrites, 0);
    }
}