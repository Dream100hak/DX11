#include "00. Global.fx"

cbuffer ParticleBuffer
{
	// for when the emit position/direction is varying
    float3 EmitPosW;
    float3 EmitDirW;

    float GameTime;
    float TimeStep;

};

cbuffer ParticleSetting
{
	// Net constant acceleration used to accerlate the particles.
    float3 AccelW = { -1.0f, -9.8f, 0.0f };
};

Texture2DArray TexArray;
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
#define PT_FLARE 1

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
                p.Type = PT_FLARE;

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
	CompileShader(gs_5_0, StreamOutGS()), "POS.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; TYPE.x");


