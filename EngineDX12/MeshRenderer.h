#pragma once
#include "Renderer.h"
#include "Material.h"

class D3D12Device;

// 절차적 프리미티브 종류 (씬 직렬화 시 스폰 오브젝트 재생성용)
enum class MeshPrim : uint8 { None, Cube, Sphere, Plane, Cylinder, Cone, Torus, Capsule };

// DX11 Engine/MeshRenderer 이식 — 정적 메시 렌더러.
// per-object 트랜스폼: ModelScene 과 동일하게 GameObject 월드행렬로 **CPU 정점 베이크**(gMVP=VP)
// → 셰이더/루트시그 변경 없이 임의 위치 메시 드로우. (정점색 경로, useTex=0)
class MeshRenderer : public Renderer
{
public:
	MeshRenderer() : Renderer(RendererType::Mesh) {}
	void Bind(D3D12Device* dev) { _dev = dev; }
	void SetGeometry(const vector<Vtx>& verts, const vector<uint32>& indices); // 로컬 정점/인덱스
	void UpdateVertices(const vector<Vtx>& verts); // 정점 수 동일 시 in-place 재업로드(터레인 스컬프트)
	void SetTexture(const std::wstring& path);                                  // 파일 → 디퓨즈 SRV
	void SetTexturePixels(const vector<uint8_t>& rgba, uint32 w, uint32 h);     // 픽셀 → 디퓨즈 SRV

	virtual void Draw(const RenderContext& ctx) override;
	virtual void RecordOutline(ID3D12GraphicsCommandList4* cmd) override;
	virtual void RecordVelocity(ID3D12GraphicsCommandList4* cmd) override; // 정적/강체 — prev=cur VB(카메라 속도만)
	virtual void TransformBoundingBox() override;
	virtual void OnInspectorGUI() override; // 머티리얼/텍스처 편집

	// RT 통합 — 월드 갱신(베이크) + 자체 BLAS 빌드 (AS 패스에서 호출, Draw 전)
	void UpdateWorld();
	void RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd);
	D3D12_GPU_VIRTUAL_ADDRESS BlasAddr() const { return _blas ? _blas->GetGPUVirtualAddress() : 0; }

	Material& GetMaterial() { return *_material; }
	shared_ptr<Material> GetMaterialRef() const { return _material; }
	void SetMaterialRef(shared_ptr<Material> m) { if (m) { _material = m; _baked = false; } } // 공유 머티리얼 지정
	const vector<Vtx>&    GetLocalVerts() const { return _local; }   // 복제용
	const vector<uint32>& GetLocalIndices() const { return _indices; }
	const vector<Vtx>&    GetWorldVerts() const { return _world; }   // RT 집계 페치용 (월드 베이크)
	// RT 집계 GPU 복사용 — 자체 월드 VB/IB 리소스 + 개수
	ID3D12Resource* VbRes() const { return _vb.Get(); }
	ID3D12Resource* IbRes() const { return _ib.Get(); }
	uint32 VtxCount() const { return (uint32)_world.size(); }
	uint32 IdxCount() const { return (uint32)_indices.size(); }
	void     SetPrim(MeshPrim p) { _prim = p; } // 직렬화/재생성용 프리미티브 종류
	MeshPrim GetPrim() const { return _prim; }

private:
	void Rebake();         // 트랜스폼/머티리얼 변경 시에만 월드 정점 재생성
	void SyncMaterialTex(); // _material 의 diffuse/normal/spec 경로 변경 시 자동 SRV 재로드
	// 디퓨즈/노멀/스펙 RGBA → 3-SRV 힙 (t2/t3/t4). npx/spx=null 이면 평평노멀/흰색 폴백.
	void BuildSrvHeap3(const uint8_t* dpx, uint32 dw, uint32 dh,
	                   const uint8_t* npx, uint32 nw, uint32 nh,
	                   const uint8_t* spx, uint32 sw, uint32 sh);

	D3D12Device*             _dev = nullptr;
	shared_ptr<Material>     _material = make_shared<Material>(); // 공유 가능(.mat 자산)
	Vec3                     _lastBakedDiffuse{ -1,-1,-1 };       // 공유 머티리얼 외부 편집 감지
	uint32                   _bakedVer = 0;
	bool                     _baked = false;
	char                     _matPathBuf[260] = {};               // .mat 경로 입력
	vector<Vtx>              _local;     // 로컬 공간 원본
	vector<uint32>          _indices;
	vector<Vtx>             _world;      // 월드 베이크 스크래치
	ComPtr<ID3D12Resource>  _vb, _ib;
	void*                   _vbMapped = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};

	// 텍스처 (슬롯당 디퓨즈/노멀/스펙 3 SRV — 메인 셰이더 t2/t3/t4 와 동일 레이아웃)
	ComPtr<ID3D12Resource>       _tex, _texN, _texS;
	ComPtr<ID3D12DescriptorHeap> _srvHeap;
	UINT                         _srvInc = 0;
	bool                         _hasTex = false;
	std::wstring                 _loadedTexPath, _loadedNormalPath, _loadedSpecPath; // 현재 SRV 경로 (변경 감지)
	char                         _texPathBuf[260] = {}; // 인스펙터 입력 버퍼
	MeshPrim                     _prim = MeshPrim::None; // 절차적 프리미티브 종류
	ComPtr<ID3D12Resource>       _blas, _blasScratch;   // RT 통합 TLAS 인스턴스용
	bool                         _blasDirty = true;     // 월드 변경 시에만 BLAS 재빌드(정적 최적화)
	bool                         _blasBuilt = false;    // 초기 빌드 완료 추적
};
