#pragma once
#include "Common.h"
#include "MeshLoader.h"
#include <string>
#include <vector>

class D3D12Device;

// ───────────────────────────────────────────────────────────
// ModelScene — 바닥(평면/절차 터레인) 지오메트리 + 통합 레이트레이싱 TLAS 소유.
//  · 바닥 VB/IB 와 BLAS, 그리고 모든 GameObject 인스턴스를 모으는 TLAS 인프라.
//  · (구버전엔 showcase 스키닝 모델도 보유했으나, 모델은 ModelAnimator GameObject 로 분리됨.)
// GPU 인프라는 D3D12Device 가 소유 — 백포인터(_dev, friend)로 접근. 상태는 public.
// ───────────────────────────────────────────────────────────
class ModelScene
{
public:
	void Init(D3D12Device* dev) { _dev = dev; }
	void Load(const std::wstring& meshPath);   // 바닥 지오메트리 + AS (재)구성. meshPath=메인 모델 스폰 경로(_modelStem) 보존용

	void RecordBuildAS(ID3D12GraphicsCommandList4* cmd);        // 바닥 BLAS + TLAS (초기 빌드)
	void RecordBuildModelBLAS(ID3D12GraphicsCommandList4* cmd); // 바닥 BLAS (인스턴스 0)
	void BuildTLAS(ID3D12GraphicsCommandList4* cmd, const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances); // 통합 TLAS
	static const UINT MAX_INSTANCES = 64;

	// ── 지오메트리 버퍼 (바닥) ──
	ComPtr<ID3D12Resource>            _vb;
	D3D12_VERTEX_BUFFER_VIEW          _vbv{};
	ComPtr<ID3D12Resource>            _ib;
	D3D12_INDEX_BUFFER_VIEW           _ibv{};
	UINT                              _indexCount = 0;
	UINT                              _vertexCount = 0;
	void*                             _vbMapped = nullptr;   // VB 영속 매핑
	uint32                            _modelIndexCount = 0;  // 항상 0 (모델 분리) — ModelRenderer 바닥 드로우 오프셋 호환
	std::vector<Vtx>                  _cpuVerts;             // 바닥 CPU 미러
	std::vector<uint32>               _cpuIndices;           // 바닥 인덱스 CPU 미러 (RT 집계 페치용)

	// ── 가속구조 (BLAS/TLAS) ──
	ComPtr<ID3D12Resource>            _blas, _blasScratch, _tlas, _tlasScratch, _instanceBuffer;
	void*                             _instanceMapped = nullptr; // 인스턴스 desc 영속 매핑
	UINT                              _instanceCount = 1;        // 마지막 TLAS 인스턴스 수

	// ── 월드 AABB (카메라 포커스 기본값) ──
	DirectX::XMFLOAT3                 _modelMin{}, _modelMax{};

	// ── 이름 / 빌드 옵션 ──
	std::wstring                      _modelDir, _modelStem; // 메인 모델 스폰 경로
	bool                              _terrain = false;      // V1 절차 터레인 바닥
	float                             _groundSize = 6.0f;    // U19

private:
	void CreateCubeGeometry();      // 바닥 VB/IB
	void CreateASBuffers();         // BLAS/TLAS 생성 + 초기 빌드 (1회)

	D3D12Device* _dev = nullptr;
};
