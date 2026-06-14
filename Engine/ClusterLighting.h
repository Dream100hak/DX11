#pragma once
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

class HlslShader;
class StructuredBuffer;

// ───────────────────────────────────────────────────────────
// Clustered shading — CPU 라이트 컬링 + 구조화버퍼 라이트 리스트
//
// 뷰 절두체를 16×9×24 froxel(클러스터) 격자로 분할(Z 는 로그 분포).
// CPU 에서 매 프레임 펑추얼(점/스팟) 라이트를 자기가 겹치는 클러스터에만 배정 →
// 클러스터별 라이트 인덱스 리스트를 GPU 구조화버퍼로 업로드.
// 디퍼드 라이팅 PS 는 픽셀이 속한 클러스터의 라이트만 순회 → MAX_LIGHTS(16) 제약 제거(수백 개).
//
// 디렉셔널 라이트는 화면 전체에 영향이라 클러스터 컬링 대상이 아니며 ClusterParams(b7) 에 인라인.
// GPU 바인딩:  t11 = ClusterLights(LightData[])  t12 = ClusterCounts(uint[])  t13 = ClusterIndices(uint[])  b7 = ClusterParams
// ───────────────────────────────────────────────────────────
class ClusterLighting
{
public:
	static const uint32 GRID_X = 16;
	static const uint32 GRID_Y = 9;
	static const uint32 GRID_Z = 24;
	static const uint32 CLUSTER_COUNT = GRID_X * GRID_Y * GRID_Z; // 3456
	static const uint32 MAX_LIGHTS_PER_CLUSTER = 64;
	static const uint32 MAX_PUNCTUAL = 256; // 클러스터 라이트(점/스팟) 총량 상한

	// 매 프레임: lights 를 디렉셔널/펑추얼로 분리하고, 펑추얼을 클러스터에 컬링하여 GPU 버퍼 갱신.
	void Build(const Matrix& view, const Matrix& proj, float screenW, float screenH,
	           float zNear, float zFar, const vector<LightData>& lights);

	// 디퍼드 라이팅 셰이더에 SRV(t11~t13) + ClusterParams(b7) 바인딩 / 해제.
	void Bind(shared_ptr<HlslShader> shader);
	void Unbind(shared_ptr<HlslShader> shader);

private:
	void EnsureBuffers();
	// 클러스터의 뷰공간 AABB 재계산 (투영/화면/근원거리 변경 시에만).
	void RebuildClusterAABBs(const Matrix& proj, float screenW, float screenH, float zNear, float zFar);

private:
	shared_ptr<StructuredBuffer> _lightSB;  // LightData[MAX_PUNCTUAL]
	shared_ptr<StructuredBuffer> _countSB;  // uint[CLUSTER_COUNT]
	shared_ptr<StructuredBuffer> _indexSB;  // uint[CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER]
	shared_ptr<ConstantBuffer<ClusterParamsDesc>> _paramsCB;

	// CPU 업로드 버퍼 (고정 크기)
	vector<LightData> _lightData;   // [MAX_PUNCTUAL]
	vector<uint32>    _counts;       // [CLUSTER_COUNT]
	vector<uint32>    _indices;      // [CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER]
	ClusterParamsDesc _params{};

	// 클러스터 AABB 캐시 (뷰공간)
	vector<Vec3> _aabbMin;
	vector<Vec3> _aabbMax;
	Matrix _cachedProj = Matrix::Identity;
	float  _cachedW = 0.f, _cachedH = 0.f, _cachedNear = 0.f, _cachedFar = 0.f;
	bool   _aabbValid = false;
};
