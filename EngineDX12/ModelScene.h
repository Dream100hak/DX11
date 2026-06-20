#pragma once
#include "Common.h"
#include "MeshLoader.h"
#include <string>
#include <vector>

class D3D12Device;

// ───────────────────────────────────────────────────────────
// ModelScene — 렌더 대상 지오메트리(스키닝 모델 + 바닥/터레인) 일체.
//  · .mesh/.clip/.mmat 로드, CPU 스키닝(매 프레임 VB 갱신), PBR 텍스처 SRV 힙,
//    레이트레이싱 가속구조(BLAS/TLAS) 소유.
//  · 모델+바닥은 하나의 VB/IB 로 합쳐지고 그대로 BLAS 소스가 된다(래스터·RT 일치).
// GPU 인프라는 D3D12Device 가 소유 — 백포인터(_dev, friend)로 접근. 상태는 public.
// (전환 단계 이름: Engine식 GameObject 컨테이너 Scene 과 구분. 추후 렌더러 컴포넌트로 분해 예정)
// ───────────────────────────────────────────────────────────
class ModelScene
{
public:
	void Init(D3D12Device* dev) { _dev = dev; }
	void Load(const std::wstring& meshPath);   // 런타임 모델 교체 (메시/텍스처/AS 재구성)
	void ScanClips();                          // 모델 폴더 .clip 목록 스캔

	// 매 프레임 CPU 스키닝(or 바인드) → 기즈모 트랜스폼 → VB 갱신 + 월드 AABB/본 위치
	void UpdateAnimation(float animTimeAcc, bool turntable, float turnAngle);

	void RecordBuildAS(ID3D12GraphicsCommandList4* cmd); // 모델 단독 BLAS+TLAS (초기 빌드/폴백)
	void RecordBuildModelBLAS(ID3D12GraphicsCommandList4* cmd);                              // 모델+바닥 BLAS 만
	void BuildTLAS(ID3D12GraphicsCommandList4* cmd, const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances); // 통합 TLAS
	static const UINT MAX_INSTANCES = 64;

	// ── 지오메트리 버퍼 ──
	ComPtr<ID3D12Resource>            _vb;
	D3D12_VERTEX_BUFFER_VIEW          _vbv{};
	ComPtr<ID3D12Resource>            _ib;
	D3D12_INDEX_BUFFER_VIEW           _ibv{};
	UINT                              _indexCount = 0;
	UINT                              _vertexCount = 0;
	void*                             _vbMapped = nullptr;   // VB 영속 매핑
	uint32                            _modelVtxCount = 0;
	uint32                            _modelIndexCount = 0;  // 모델 인덱스 수(바닥 제외)
	std::vector<Vtx>                  _cpuVerts;             // 합본(모델+바닥) CPU 미러
	std::vector<uint32>               _cpuIndices;           // 합본(모델+바닥) 인덱스 CPU 미러 (RT 집계 페치용)

	// ── 가속구조 (BLAS/TLAS) ──
	ComPtr<ID3D12Resource>            _blas, _blasScratch, _tlas, _tlasScratch, _instanceBuffer;
	void*                             _instanceMapped = nullptr; // 인스턴스 desc 영속 매핑
	UINT                              _instanceCount = 1;        // 마지막 TLAS 인스턴스 수

	// ── 스키닝 / 애니메이션 ──
	std::vector<LoadedBone>           _bonesData;
	std::vector<SkinVtx>              _skinSrc;
	AnimClip                          _clip;
	bool                              _animated = false;
	float                             _modelScale = 1.f;
	DirectX::XMFLOAT3                 _modelOffset{ 0, 0, 0 };
	std::vector<DirectX::XMFLOAT3>    _boneWorld;            // 본 월드 위치(스켈레톤 시각화)
	std::vector<std::wstring>         _clips; int _curClip = 0;

	// ── 트랜스폼 / 월드 AABB(픽킹) ──
	DirectX::XMFLOAT4X4               _modelMatrix;          // 기즈모 트랜스폼
	bool                              _modelMatrixInit = false;
	DirectX::XMFLOAT3                 _modelMin{}, _modelMax{};

	// ── 텍스처 / 머티리얼 ──
	std::vector<ComPtr<ID3D12Resource>> _matResources;       // 슬롯×3 (생존 유지)
	ComPtr<ID3D12Resource>            _whiteTex;             // 폴백(1×1 흰색)
	ComPtr<ID3D12DescriptorHeap>      _srvHeap;
	UINT                              _srvInc = 0;
	UINT                              _matCount = 0;
	std::vector<SubMesh>              _submeshes;
	std::vector<uint32>               _subMatSlot;
	bool                              _hasTexture = false;

	// ── 이름 / 빌드 옵션 ──
	std::wstring                      _modelDir, _modelStem;
	std::wstring                      _modelLabel = L"Archer";
	bool                              _floorOnly = false;    // true=모델 미로드(바닥+TLAS만). 모델은 ModelAnimator GameObject 로 분리
	bool                              _terrain = false;      // V1 절차 터레인
	float                             _groundSize = 6.0f;    // U19

private:
	void CreateCubeGeometry();      // 메시 + 바닥 + VB/IB
	void CreateTextureResources();  // .mmat/.mat → 머티리얼 텍스처 + SRV 힙
	void CreateASBuffers();         // BLAS/TLAS 생성 + 초기 빌드 (1회)

	D3D12Device* _dev = nullptr;
};
