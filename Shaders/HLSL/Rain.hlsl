// Rain.hlsl
// 비 파티클 (FX 01. Rain.fx 대체)
//  - SO 패스:  VS_StreamOut + GS_StreamOut (Stream-Output) — 카메라 위에서 빗방울 생성
//  - Draw 패스: VS_Draw + GS_Draw(라인 확장) + PS_Draw
// b0: GlobalBuffer (VP) — Common.hlsli
// b8: ParticleBuffer, t0: TexArray (PS), t1: RandomTex (GS)

#include "Common.hlsli"

#define PT_EMITTER 0
#define PT_RAIN    1

cbuffer ParticleBuffer : register(b8)
{
    float3 EmitPosW;
    float  GameTime;
    float3 EmitDirW;
    float  TimeStep;
    float3 AccelW;        // 가속도 (기존 static const gAccelW)
    float  EmitInterval;  // 방출 주기 (초)
    float  Lifetime;      // 입자 수명 (초)
    float  InitialSpeed;  // 분산 반경 (카메라 주변 스폰 범위)
    float2 ParticleSize;  // 빗방울 크기
};

Texture2DArray TexArray  : register(t0);
Texture1D      RandomTex : register(t1);

float3 RandVec3(float offset)
{
    float u = (GameTime + offset);
    return RandomTex.SampleLevel(LinearSampler, u, 0).xyz;
}

// ===========================================================
// Stream-Out 패스
// ===========================================================
struct VertexParticle
{
    float3 InitialPosW : POS;
    float3 InitialVelW : VELOCITY;
    float2 SizeW       : SIZE;
    float  Age         : AGE;
    uint   Type        : TYPE;
};

VertexParticle VS_StreamOut(VertexParticle vin)
{
    return vin;
}

[maxvertexcount(6)]
void GS_StreamOut(point VertexParticle gin[1],
    inout PointStream<VertexParticle> ptStream)
{
    gin[0].Age += TimeStep;

    if (gin[0].Type == PT_EMITTER)
    {
        if (gin[0].Age > EmitInterval)
        {
            for (int i = 0; i < 5; ++i)
            {
                // 카메라 위쪽 영역에 빗방울 분산
                float3 vRandom = InitialSpeed * RandVec3((float)i / 5.0f);
                vRandom.y = 20.0f;

                VertexParticle p;
                p.InitialPosW = EmitPosW.xyz + vRandom;
                p.InitialVelW = float3(0.0f, 0.0f, 0.0f);
                p.SizeW = ParticleSize;
                p.Age = 0.0f;
                p.Type = PT_RAIN;

                ptStream.Append(p);
            }

            gin[0].Age = 0.0f;
        }

        ptStream.Append(gin[0]);
    }
    else
    {
        if (gin[0].Age <= Lifetime)
            ptStream.Append(gin[0]);
    }
}

// ===========================================================
// Draw 패스 — 포인트를 가속 방향 라인으로 확장
// ===========================================================
struct VertexOut
{
    float3 PosW : POS;
    uint   Type : TYPE;
};

VertexOut VS_Draw(VertexParticle vin)
{
    VertexOut vout;

    float t = vin.Age;
    vout.PosW = 0.5f * t * t * AccelW + t * vin.InitialVelW + vin.InitialPosW;
    vout.Type = vin.Type;

    return vout;
}

struct GeoOut
{
    float4 PosH : SV_Position;
    float2 Tex  : TEXCOORD;
};

[maxvertexcount(2)]
void GS_Draw(point VertexOut gin[1],
    inout LineStream<GeoOut> lineStream)
{
    if (gin[0].Type != PT_EMITTER)
    {
        // 가속 방향으로 기울어진 라인
        float3 p0 = gin[0].PosW;
        float3 p1 = gin[0].PosW + 0.07f * AccelW;

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

float4 PS_Draw(GeoOut pin) : SV_TARGET
{
    return TexArray.Sample(LinearSampler, float3(pin.Tex, 0));
}
