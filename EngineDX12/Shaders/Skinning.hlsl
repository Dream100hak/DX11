// GPU 스키닝 컴퓨트 — 본 행렬 × 소스(바인드포즈) 정점 → 월드공간 출력 정점.
// CPU 스키닝(ModelAnimator::SkinVerts) 대체. 본 행렬은 CPU 에서 포즈 블렌딩까지 마친 결과를 업로드.
// 레이아웃은 C++ SkinVtx / Vtx 와 바이트 단위로 일치해야 함.

cbuffer SkinParams : register(b0)
{
	row_major float4x4 gModelToWorld; // 스킨 로컬 위치 → 월드 (오프셋/스케일/배치행렬 베이크)
	row_major float4x4 gWorld;        // 노멀/탄젠트 방향 변환 (배치행렬 회전부)
	uint  gVtxCount;
	uint  gBoneCount;
	uint2 _pad;
};

// 소스 정점 (C++ SkinVtx = 76B: pos12 nrm12 tan12 uv8 idx16 wgt16)
struct SkinSrc
{
	float3 pos;
	float3 nrm;
	float3 tan;
	float2 uv;
	uint4  idx;
	float4 wgt;
};

// 본 스킨 행렬 (row_major 강제 — C++ XMMATRIX 행우선과 일치)
struct Bone { row_major float4x4 m; };

// 출력 정점 (C++ Vtx = 56B: pos12 nrm12 col12 uv8 tan12)
struct OutVtx
{
	float3 pos;
	float3 nrm;
	float3 col;
	float2 uv;
	float3 tan;
};

StructuredBuffer<SkinSrc>   gSrc   : register(t0);
StructuredBuffer<Bone>      gBones : register(t1);
RWStructuredBuffer<OutVtx>  gOut   : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	uint i = id.x;
	if (i >= gVtxCount) return;

	SkinSrc s = gSrc[i];
	float3 p = 0, n = 0, t = 0;
	float wsum = 0;
	[unroll]
	for (int j = 0; j < 4; ++j)
	{
		float w = s.wgt[j];
		uint  bi = s.idx[j];
		if (w <= 0.0 || bi >= gBoneCount) continue;
		float4x4 m = gBones[bi].m;
		p += w * mul(float4(s.pos, 1), m).xyz;
		n += w * mul(float4(s.nrm, 0), m).xyz;
		t += w * mul(float4(s.tan, 0), m).xyz;
		wsum += w;
	}
	if (wsum < 1e-4) { p = s.pos; n = s.nrm; t = s.tan; }

	OutVtx o;
	o.pos = mul(float4(p, 1), gModelToWorld).xyz;
	o.nrm = normalize(mul(float4(n, 0), gWorld).xyz);
	o.tan = normalize(mul(float4(t, 0), gWorld).xyz);
	o.col = float3(0.82, 0.78, 0.72);
	o.uv  = s.uv;
	gOut[i] = o;
}
