#pragma once
#include "Renderer.h"
#include "Material.h"

class D3D12Device;

// DX11 Engine/MeshRenderer 이식 — 정적 메시 렌더러.
// per-object 트랜스폼: ModelScene 과 동일하게 GameObject 월드행렬로 **CPU 정점 베이크**(gMVP=VP)
// → 셰이더/루트시그 변경 없이 임의 위치 메시 드로우. (정점색 경로, useTex=0)
class MeshRenderer : public Renderer
{
public:
	MeshRenderer() : Renderer(RendererType::Mesh) {}
	void Bind(D3D12Device* dev) { _dev = dev; }
	void SetGeometry(const vector<Vtx>& verts, const vector<uint32>& indices); // 로컬 정점/인덱스
	void SetTexture(const std::wstring& path);                                  // 파일 → 디퓨즈 SRV
	void SetTexturePixels(const vector<uint8_t>& rgba, uint32 w, uint32 h);     // 픽셀 → 디퓨즈 SRV

	virtual void Draw(const RenderContext& ctx) override;
	virtual void TransformBoundingBox() override;
	virtual void OnInspectorGUI() override; // 머티리얼/텍스처 편집

	Material& GetMaterial() { return _material; }
	const vector<Vtx>&    GetLocalVerts() const { return _local; }   // 복제용
	const vector<uint32>& GetLocalIndices() const { return _indices; }

private:
	void Rebake();         // 트랜스폼/머티리얼 변경 시에만 월드 정점 재생성
	void SyncMaterialTex(); // _material._diffuseTex 경로 변경 시 자동 SRV 로드

	D3D12Device*             _dev = nullptr;
	Material                 _material;
	uint32                   _bakedVer = 0;
	bool                     _baked = false;
	vector<Vtx>              _local;     // 로컬 공간 원본
	vector<uint32>          _indices;
	vector<Vtx>             _world;      // 월드 베이크 스크래치
	ComPtr<ID3D12Resource>  _vb, _ib;
	void*                   _vbMapped = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};

	// 텍스처 (슬롯당 디퓨즈/노멀/스펙 3 SRV — 메인 셰이더 t2/t3/t4 와 동일 레이아웃)
	ComPtr<ID3D12Resource>       _tex, _white;
	ComPtr<ID3D12DescriptorHeap> _srvHeap;
	UINT                         _srvInc = 0;
	bool                         _hasTex = false;
	std::wstring                 _loadedTexPath; // 현재 SRV 에 올라간 경로 (변경 감지)
	char                         _texPathBuf[260] = {}; // 인스펙터 입력 버퍼
};
