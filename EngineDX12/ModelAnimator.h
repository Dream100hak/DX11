#pragma once
#include "Renderer.h"
#include "MeshLoader.h"
#include "Material.h"
#include <string>

class D3D12Device;

// DX11 Engine/ModelAnimator 이식 — per-GameObject 스키닝 모델 렌더러.
// 자체 .mesh/.clip 로드 + 본/블렌드, 매 프레임 CPU 스키닝 → GameObject 월드행렬로 베이크한
// 월드 VB 드로우. ModelScene(단일 모델)과 독립 — 여러 애니 캐릭터 가능.
// Stage 1a: 정점색(useTex=0) 래스터. (1b 머티리얼, 2 통합 TLAS RT)
class ModelAnimator : public Renderer
{
public:
	ModelAnimator() : Renderer(RendererType::Animator) {}
	void Bind(D3D12Device* dev) { _dev = dev; }
	bool Load(const std::wstring& meshPath); // 메시+본+클립 로드, 월드 VB/IB 생성

	uint32 IndexCount() const { return _idxCount; } // RT TLAS OOB 방어 가드용
	virtual void Draw(const RenderContext& ctx) override;
	virtual void RecordOutline(ID3D12GraphicsCommandList4* cmd) override;
	virtual void TransformBoundingBox() override;
	virtual void OnInspectorGUI() override;

	// RT 통합 — 시간 전진+스키닝(AS 패스, Draw 전) + 자체 BLAS 빌드
	void UpdateWorld();
	void RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd);
	D3D12_GPU_VIRTUAL_ADDRESS BlasAddr() const { return _blas ? _blas->GetGPUVirtualAddress() : 0; }

	const std::wstring& MeshDir()  const { return _modelDir; }
	const std::wstring& MeshStem() const { return _modelStem; }
	int  GetClipIndex() const { return _curClip; }
	void SetClipIndex(int i);
	float GetSpeed() const { return _speed; }
	void  SetSpeed(float s) { _speed = s; }
	bool  IsPlaying() const { return _playing; }
	void  SetPlaying(bool p) { _playing = p; }

	// 머티리얼 오버라이드 (모델 전체 — 텍스처는 .mmat 슬롯 유지, PBR/틴트는 이 머티리얼)
	Material& GetMaterial() { return *_material; }
	shared_ptr<Material> GetMaterialRef() const { return _material; }
	void SetMaterialRef(shared_ptr<Material> m) { if (m) _material = m; }

private:
	void ScanClips();
	void Skin(uint32 frame); // 지정 프레임 스키닝 → _world → VB
	void CreateMaterials();  // .mmat → 슬롯별 디퓨즈/노멀/스펙 SRV 힙

	D3D12Device* _dev = nullptr;

	// 로드 데이터
	std::vector<LoadedBone> _bonesData;
	std::vector<SkinVtx>    _skinSrc;
	std::vector<uint32>     _indices;
	std::vector<SubMesh>    _submeshes;
	AnimClip                _clip;
	std::vector<std::wstring> _clips; int _curClip = 0;
	bool                    _animated = false;
	float                   _modelScale = 1.f;
	DirectX::XMFLOAT3       _modelOffset{ 0,0,0 };
	std::wstring            _modelDir, _modelStem;

	// 재생 상태
	float _animTime = 0.f;
	float _speed = 1.f;
	bool  _playing = true;
	bool  _loop = true;

	// GPU 버퍼 (월드 베이크 VB = 영속 매핑)
	std::vector<Vtx>         _world;
	ComPtr<ID3D12Resource>   _vb, _ib;
	void*                    _vbMapped = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};
	uint32                   _vtxCount = 0, _idxCount = 0;
	DirectX::XMFLOAT3        _aabbMin{}, _aabbMax{};

	// 머티리얼 (슬롯×3 디퓨즈/노멀/스펙 — 모델 셰이더 t2/t3/t4)
	std::vector<ComPtr<ID3D12Resource>> _matResources;
	ComPtr<ID3D12Resource>              _whiteTex;
	ComPtr<ID3D12Resource>              _flatNormalTex; // 노멀맵 없는 슬롯 폴백 (128,128,255=(0,0,1))
	ComPtr<ID3D12DescriptorHeap>        _srvHeap;
	UINT                                _srvInc = 0;
	uint32                              _matCount = 0;
	std::vector<uint32>                 _subMatSlot;
	bool                                _hasTexture = false;
	shared_ptr<Material>                _material = make_shared<Material>(); // 모델 PBR/틴트 오버라이드

	ComPtr<ID3D12Resource>              _blas, _blasScratch; // RT 통합 TLAS 인스턴스용
	bool                                _blasDirty = true;   // 스키닝 변경 시에만 BLAS 재빌드
	uint32                              _bakedVer = 0;       // 트랜스폼 더티 체크
	uint32                              _lastFrame = 0xFFFFFFFF; // 마지막 스키닝 프레임(변화 없으면 스킵)
	bool                                _skinnedOnce = false;
};
