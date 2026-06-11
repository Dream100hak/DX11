// Fire.hlsl
// 불꽃 파티클 (FX 01. Fire.fx 대체)
//  - SO 패스:  VS_StreamOut + GS_StreamOut (Stream-Output, 래스터라이즈 없음) — 입자 생성/소멸
//  - Draw 패스: VS_Draw + GS_Draw(빌보드 확장) + PS_Draw (가산 블렌드)
// b0: GlobalBuffer (VP, VInv) — Common.hlsli
// b8: ParticleBuffer (EmitPos/Dir, GameTime, TimeStep)
// t0: TexArray (PS), t1: RandomTex (GS)

#include "Common.hlsli"

#define PT_EMITTER 0
#define PT_FLARE   1

cbuffer ParticleBuffer : register(b8)
{
    float3 EmitPosW;
    float  GameTime;
    float3 EmitDirW;
    float  TimeStep;
    float3 AccelW;        // 가속도 (기존 static const gAccelW)
    float  EmitInterval;  // 방출 주기 (초)
    float  Lifetime;      // 입자 수명 (초)
    float  InitialSpeed;  // 초기 속도 배율
    float2 ParticleSize;  // 빌보드 크기
};

// 고정 상수 (FX cbFixed 대체)
static const float2 gQuadTexC[4] =
{
    float2(0.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f)
};

Texture2DArray TexArray  : register(t0);
Texture1D      RandomTex : register(t1);

float3 RandUnitVec3(float offset)
{
    float u = (GameTime + offset);
    float3 v = RandomTex.SampleLevel(LinearSampler, u, 0).xyz;
    return normalize(v);
}

// ===========================================================
// Stream-Out 패스 — 입자 생성/소멸만 담당
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

[maxvertexcount(2)]
void GS_StreamOut(point VertexParticle gin[1],
    inout PointStream<VertexParticle> ptStream)
{
    gin[0].Age += TimeStep;

    if (gin[0].Type == PT_EMITTER)
    {
        // 방출 주기 도달?
        if (gin[0].Age > EmitInterval)
        {
            float3 vRandom = RandUnitVec3(0.0f);
            vRandom.x *= 0.5f;
            vRandom.z *= 0.5f;

            VertexParticle p;
            p.InitialPosW = EmitPosW.xyz;
            p.InitialVelW = InitialSpeed * vRandom;
            p.SizeW = ParticleSize;
            p.Age = 0.0f;
            p.Type = PT_FLARE;

            ptStream.Append(p);

            gin[0].Age = 0.0f;
        }

        // 이미터는 항상 유지
        ptStream.Append(gin[0]);
    }
    else
    {
        // 수명 내 입자만 유지
        if (gin[0].Age <= Lifetime)
            ptStream.Append(gin[0]);
    }
}

// ===========================================================
// Draw 패스 — 포인트를 카메라 빌보드 콰드로 확장
// ===========================================================
struct VertexOut
{
    float3 PosW  : POS;
    float2 SizeW : SIZE;
    float4 Color : COLOR;
    uint   Type  : TYPE;
};

VertexOut VS_Draw(VertexParticle vin)
{
    VertexOut vout;

    float t = vin.Age;

    // 등가속도 운동
    vout.PosW = 0.5f * t * t * AccelW + t * vin.InitialVelW + vin.InitialPosW;

    // 시간에 따라 페이드 (수명 기준)
    float opacity = 1.0f - smoothstep(0.0f, 1.0f, t / Lifetime);
    vout.Color = float4(1.0f, 1.0f, 1.0f, opacity);

    vout.SizeW = vin.SizeW;
    vout.Type = vin.Type;

    return vout;
}

struct GeoOut
{
    float4 PosH  : SV_Position;
    float4 Color : COLOR;
    float2 uv    : TEXCOORD;
};

[maxvertexcount(4)]
void GS_Draw(point VertexOut gin[1],
    inout TriangleStream<GeoOut> triStream)
{
    // 이미터는 그리지 않음
    if (gin[0].Type != PT_EMITTER)
    {
        // 카메라를 향하는 빌보드
        float3 look  = normalize(CameraPositionWS() - gin[0].PosW);
        float3 right = normalize(cross(float3(0, 1, 0), look));
        float3 up    = cross(look, right);

        float halfWidth  = 0.5f * gin[0].SizeW.x;
        float halfHeight = 0.5f * gin[0].SizeW.y;

        float4 v[4];
        v[0] = float4(gin[0].PosW + halfWidth * right - halfHeight * up, 1.0f);
        v[1] = float4(gin[0].PosW + halfWidth * right + halfHeight * up, 1.0f);
        v[2] = float4(gin[0].PosW - halfWidth * right - halfHeight * up, 1.0f);
        v[3] = float4(gin[0].PosW - halfWidth * right + halfHeight * up, 1.0f);

        GeoOut gout;
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            gout.PosH  = mul(v[i], VP);
            gout.uv    = gQuadTexC[i];
            gout.Color = gin[0].Color;
            triStream.Append(gout);
        }
    }
}

float4 PS_Draw(GeoOut pin) : SV_TARGET
{
    return TexArray.Sample(LinearSampler, float3(pin.uv, 0)) * pin.Color;
}
