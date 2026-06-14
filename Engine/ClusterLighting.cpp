#include "pch.h"
#include "ClusterLighting.h"
#include "HlslShader.h"
#include "StructuredBuffer.h"
#include <float.h>

void ClusterLighting::EnsureBuffers()
{
	if (_lightSB)
		return;

	_lightData.assign(MAX_PUNCTUAL, LightData{});
	_counts.assign(CLUSTER_COUNT, 0);
	_indices.assign(CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER, 0);

	// SRV 전용(동적) 구조화버퍼 — StructuredBuffer 의 입력 경로(D3D11_USAGE_DYNAMIC)만 사용.
	_lightSB = make_shared<StructuredBuffer>(_lightData.data(), sizeof(LightData), MAX_PUNCTUAL);
	_countSB = make_shared<StructuredBuffer>(_counts.data(), sizeof(uint32), CLUSTER_COUNT);
	_indexSB = make_shared<StructuredBuffer>(_indices.data(), sizeof(uint32), CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER);

	_paramsCB = make_shared<ConstantBuffer<ClusterParamsDesc>>();
	_paramsCB->Create();

	_aabbMin.resize(CLUSTER_COUNT);
	_aabbMax.resize(CLUSTER_COUNT);
}

// 화면 UV(0~1) → 뷰공간 방향 벡터 (원점에서 해당 픽셀로). z>0.
static Vec3 ScreenToViewDir(float u, float v, const Matrix& invProj)
{
	float ndcX = u * 2.f - 1.f;
	float ndcY = 1.f - v * 2.f; // DX: 화면 위(v=0) → ndcY=+1
	Vec4 clip(ndcX, ndcY, 1.f, 1.f);
	Vec4 view = Vec4::Transform(clip, invProj);
	float invW = (fabsf(view.w) > 1e-8f) ? (1.f / view.w) : 1.f;
	return Vec3(view.x * invW, view.y * invW, view.z * invW);
}

void ClusterLighting::RebuildClusterAABBs(const Matrix& proj, float screenW, float screenH, float zNear, float zFar)
{
	Matrix invProj = proj.Invert();
	const float ratio = zFar / zNear;

	for (uint32 cz = 0; cz < GRID_Z; ++cz)
	{
		float nearD = zNear * powf(ratio, (float)cz / (float)GRID_Z);
		float farD  = zNear * powf(ratio, (float)(cz + 1) / (float)GRID_Z);

		for (uint32 cy = 0; cy < GRID_Y; ++cy)
		{
			for (uint32 cx = 0; cx < GRID_X; ++cx)
			{
				Vec3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
				Vec3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);

				for (int corner = 0; corner < 4; ++corner)
				{
					float u = (float)(cx + (corner & 1)) / (float)GRID_X;
					float v = (float)(cy + ((corner >> 1) & 1)) / (float)GRID_Y;
					Vec3 dir = ScreenToViewDir(u, v, invProj);
					if (fabsf(dir.z) < 1e-6f) dir.z = 1e-6f;

					Vec3 pN = dir * (nearD / dir.z);
					Vec3 pF = dir * (farD / dir.z);

					mn = Vec3::Min(mn, Vec3::Min(pN, pF));
					mx = Vec3::Max(mx, Vec3::Max(pN, pF));
				}

				uint32 idx = cx + cy * GRID_X + cz * GRID_X * GRID_Y;
				_aabbMin[idx] = mn;
				_aabbMax[idx] = mx;
			}
		}
	}

	_cachedProj = proj;
	_cachedW = screenW;
	_cachedH = screenH;
	_cachedNear = zNear;
	_cachedFar = zFar;
	_aabbValid = true;
}

// 구(중심 c, 반경 r) vs AABB 교차 — 최근접점 제곱거리 비교.
static bool SphereAabb(const Vec3& c, float r, const Vec3& mn, const Vec3& mx)
{
	float sq = 0.f;
	if (c.x < mn.x) sq += (mn.x - c.x) * (mn.x - c.x); else if (c.x > mx.x) sq += (c.x - mx.x) * (c.x - mx.x);
	if (c.y < mn.y) sq += (mn.y - c.y) * (mn.y - c.y); else if (c.y > mx.y) sq += (c.y - mx.y) * (c.y - mx.y);
	if (c.z < mn.z) sq += (mn.z - c.z) * (mn.z - c.z); else if (c.z > mx.z) sq += (c.z - mx.z) * (c.z - mx.z);
	return sq <= r * r;
}

void ClusterLighting::Build(const Matrix& view, const Matrix& proj, float screenW, float screenH,
                            float zNear, float zFar, const vector<LightData>& lights)
{
	EnsureBuffers();

	// 1) 디렉셔널 / 펑추얼 분리
	uint32 dirCount = 0;
	uint32 punctualCount = 0;
	for (const LightData& ld : lights)
	{
		if (ld.type == 0) // Directional
		{
			if (dirCount < MAX_CLUSTER_DIR_LIGHTS)
				_params.dirLights[dirCount++] = ld;
		}
		else // Point(1) / Spot(2)
		{
			if (punctualCount < MAX_PUNCTUAL)
				_lightData[punctualCount++] = ld;
		}
	}

	// 2) 클러스터 AABB 캐시 (투영/화면/거리 변경 시에만 재계산)
	if (!_aabbValid || _cachedProj != proj || _cachedW != screenW || _cachedH != screenH ||
		_cachedNear != zNear || _cachedFar != zFar)
	{
		RebuildClusterAABBs(proj, screenW, screenH, zNear, zFar);
	}

	// 3) 펑추얼 라이트 클러스터 컬링
	std::fill(_counts.begin(), _counts.end(), 0u);

	const float logRatio = logf(zFar / zNear);
	auto sliceFromZ = [&](float z) -> int32
	{
		float zz = (z < zNear) ? zNear : z;
		int32 s = (int32)floorf(logf(zz / zNear) / logRatio * (float)GRID_Z);
		if (s < 0) s = 0;
		if (s > (int32)GRID_Z - 1) s = (int32)GRID_Z - 1;
		return s;
	};

	for (uint32 pi = 0; pi < punctualCount; ++pi)
	{
		const LightData& ld = _lightData[pi];
		Vec3 vp = Vec3::Transform(ld.position, view); // 뷰공간 중심
		float r = ld.range;

		float zMax = vp.z + r;
		if (zMax <= zNear) continue;                  // 근평면 뒤로 완전히 벗어남
		int32 szMin = sliceFromZ(vp.z - r);
		int32 szMax = sliceFromZ(zMax);

		for (int32 cz = szMin; cz <= szMax; ++cz)
		{
			uint32 zBase = (uint32)cz * GRID_X * GRID_Y;
			for (uint32 cy = 0; cy < GRID_Y; ++cy)
			{
				for (uint32 cx = 0; cx < GRID_X; ++cx)
				{
					uint32 idx = cx + cy * GRID_X + zBase;
					if (!SphereAabb(vp, r, _aabbMin[idx], _aabbMax[idx]))
						continue;
					uint32 c = _counts[idx];
					if (c >= MAX_LIGHTS_PER_CLUSTER) continue;
					_indices[idx * MAX_LIGHTS_PER_CLUSTER + c] = pi;
					_counts[idx] = c + 1;
				}
			}
		}
	}

	// 4) GPU 업로드
	_params.gridX = GRID_X;
	_params.gridY = GRID_Y;
	_params.gridZ = GRID_Z;
	_params.maxPerCluster = MAX_LIGHTS_PER_CLUSTER;
	_params.zNear = zNear;
	_params.zFar = zFar;
	_params.punctualCount = punctualCount;
	_params.dirCount = dirCount;
	_params.screenW = screenW;
	_params.screenH = screenH;

	_lightSB->CopyToInput(_lightData.data());
	_countSB->CopyToInput(_counts.data());
	_indexSB->CopyToInput(_indices.data());
	_paramsCB->CopyData(_params);
}

void ClusterLighting::Bind(shared_ptr<HlslShader> shader)
{
	if (!shader || !_lightSB) return;
	shader->SetPSSRV(11, _lightSB->GetSRV().Get());
	shader->SetPSSRV(12, _countSB->GetSRV().Get());
	shader->SetPSSRV(13, _indexSB->GetSRV().Get());
	shader->SetPSConstantBuffer(7, _paramsCB->GetComPtr().Get());
}

void ClusterLighting::Unbind(shared_ptr<HlslShader> shader)
{
	if (!shader) return;
	shader->SetPSSRV(11, nullptr);
	shader->SetPSSRV(12, nullptr);
	shader->SetPSSRV(13, nullptr);
}
