// Ssao.hlsl
// SSAO ambient occlusion 계산 패스 (FX 00. Ssao.fx 대체)
// 입력: normal-depth 맵 (view-space normal.xyz + view-space depth.w)
// 출력: ambient access (1 = 가림 없음) — R16_FLOAT 하프 해상도 타깃
// 샘플러는 Ssao 클래스(C++)에서 직접 생성/바인딩: s0 = BORDER(0,0,0,1e5), s1 = WRAP

#define SAMPLE_COUNT 14

cbuffer SsaoBuffer : register(b8)
{
    matrix ViewToTexSpace; // Proj * Texture
    float4 OffsetVectors[14];
    float4 FrustumCorners[4];

    // Coordinates given in view space.
    float OcclusionRadius;
    float OcclusionFadeStart;
    float OcclusionFadeEnd;
    float SurfaceEpsilon;
};

Texture2D NormalDepthMap : register(t0);
Texture2D RandomVecMap   : register(t1);

SamplerState samNormalDepth : register(s0); // LINEAR_MIP_POINT, BORDER(0,0,0,1e5)
SamplerState samRandomVec   : register(s1); // LINEAR_MIP_POINT, WRAP

struct VertexSsao
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;   // x = frustum far corner index
    float2 uv     : TEXCOORD;
};

struct VertexOut
{
    float4 PosH       : SV_POSITION;
    float3 ToFarPlane : TEXCOORD0;
    float2 uv         : TEXCOORD1;
};

VertexOut VS_Main(VertexSsao vin)
{
    VertexOut vout;
    // Already in NDC space.
    vout.PosH = float4(vin.pos, 1.0f);
    // We store the index to the frustum corner in the normal x-coord slot.
    vout.ToFarPlane = FrustumCorners[vin.normal.x].xyz;
    vout.uv = vin.uv;
    return vout;
}

// q 가 p 를 얼마나 가리는지 (distZ 함수)
float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > SurfaceEpsilon)
    {
        float fadeLength = OcclusionFadeEnd - OcclusionFadeStart;
        occlusion = saturate((OcclusionFadeEnd - distZ) / fadeLength);
    }
    return occlusion;
}

float4 PS_Main(VertexOut pin) : SV_Target
{
    // p: AO 를 계산할 지점, n: p 의 노멀, q: p 주변 임의 오프셋, r: 잠재적 가림 지점
    float4 normalDepth = NormalDepthMap.SampleLevel(samNormalDepth, pin.uv, 0.0f);

    float3 n  = normalDepth.xyz;
    float  pz = normalDepth.w;

    // view-space 위치 복원: p = (pz / ToFarPlane.z) * ToFarPlane
    float3 p = (pz / pin.ToFarPlane.z) * pin.ToFarPlane;

    // 랜덤 벡터 [0,1] -> [-1,1]
    float3 randVec = 2.0f * RandomVecMap.SampleLevel(samRandomVec, 4.0f * pin.uv, 0.0f).rgb - 1.0f;

    float occlusionSum = 0.0f;

    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        // 고정 분포 오프셋을 랜덤 벡터에 반사 → 균일 랜덤 분포
        float3 offset = reflect(OffsetVectors[i].xyz, randVec);

        // (p, n) 평면 뒤쪽이면 뒤집기
        float flip = sign(dot(offset, n));

        float3 q = p + flip * OcclusionRadius * offset;

        // q 를 투영해 텍스처 좌표 생성
        float4 projQ = mul(float4(q, 1.0f), ViewToTexSpace);
        projQ /= projQ.w;

        // 시선->q 레이를 따라 가장 가까운 깊이
        float rz = NormalDepthMap.SampleLevel(samNormalDepth, projQ.xy, 0.0f).a;

        // r = (rz / q.z) * q
        float3 r = (rz / q.z) * q;

        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        occlusionSum += dp * OcclusionFunction(distZ);
    }

    occlusionSum /= SAMPLE_COUNT;

    float access = 1.0f - occlusionSum;

    // 대비 강화
    return saturate(pow(access, 4.0f));
}
