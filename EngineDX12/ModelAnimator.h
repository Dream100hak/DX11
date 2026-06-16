#pragma once
#include "Renderer.h"
#include "MeshLoader.h"
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

	virtual void Draw(const RenderContext& ctx) override;
	virtual void TransformBoundingBox() override;
	virtual void OnInspectorGUI() override;

	const std::wstring& MeshDir()  const { return _modelDir; }
	const std::wstring& MeshStem() const { return _modelStem; }
	int  GetClipIndex() const { return _curClip; }
	void SetClipIndex(int i);
	float GetSpeed() const { return _speed; }
	void  SetSpeed(float s) { _speed = s; }
	void  SetPlaying(bool p) { _playing = p; }

private:
	void ScanClips();
	void Skin(); // 현재 프레임 스키닝 → _world → VB

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
};
