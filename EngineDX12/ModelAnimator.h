#pragma once
#include "Renderer.h"
#include "MeshLoader.h"
#include "Material.h"
#include <string>
#include <unordered_map>
#include <functional>

class D3D12Device;

// ── 애니메이션 컨트롤러 구조 (Unity Animator / Unreal AnimBP 의 코드 기반 미니 버전) ──
// 본 로컬 포즈 (블렌딩 단위 — S / R(quat) / T)
struct BonePose { DirectX::XMFLOAT3 s{ 1,1,1 }; DirectX::XMFLOAT4 r{ 0,0,0,1 }; DirectX::XMFLOAT3 t{ 0,0,0 }; };

// 애니 노티파이 — 정규화 시간(0~1)에 이름 이벤트 발생 (발소리/공격판정/이펙트 트리거 등)
struct AnimNotify { float time = 0.5f; std::string name; };

// 상태머신: 상태 = (클립 + 속도 + 루프), 전이 = (조건 파라미터 + 블렌드 시간)
struct AnimState { std::string name; int clip = 0; float speed = 1.f; bool loop = true; };
struct AnimTransition
{
	int   from = -1;            // -1 = Any State (모든 상태에서)
	int   to = 0;
	float blend = 0.2f;         // 크로스페이드 시간(초)
	std::string param;          // 조건 파라미터 (빈 문자열 = 조건 없음 → ExitTime 만)
	int   op = 2;               // 0:>  1:<  2:!=0(bool/trigger true)
	float value = 0.f;
	bool  hasExitTime = false;  // 현재 클립이 exitTime 비율 이상 재생돼야 전이 허용
	float exitTime = 0.95f;
};

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
	uint32 VtxCount()   const { return _vtxCount; }
	const std::vector<Vtx>&    GetWorldVerts() const { return _world; }   // RT 집계 페치용 (월드 베이크 스킨)
	const std::vector<uint32>& GetIndices() const { return _indices; }
	// RT 집계 GPU 복사용 — 자체 월드 VB/IB 리소스
	ID3D12Resource* VbRes() const { return _vb.Get(); }
	ID3D12Resource* IbRes() const { return _ib.Get(); }
	virtual void Draw(const RenderContext& ctx) override;
	virtual void RecordOutline(ID3D12GraphicsCommandList4* cmd) override;
	virtual void RecordVelocity(ID3D12GraphicsCommandList4* cmd) override; // 스키닝 — prev=_vbPrev(애니 모션 캡처)
	virtual void TransformBoundingBox() override;
	virtual void OnInspectorGUI() override;

	// RT 통합 — 시간 전진+포즈 계산(CPU) + GPU 스키닝 디스패치 + 자체 BLAS 빌드 (AS 패스, Draw 전)
	void UpdateWorld();
	void RecordSkinning(ID3D12GraphicsCommandList4* cmd); // 본행렬 × 소스정점 → 월드 VB (컴퓨트)
	void RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd);
	D3D12_GPU_VIRTUAL_ADDRESS BlasAddr() const { return _blas ? _blas->GetGPUVirtualAddress() : 0; }

	const std::wstring& MeshDir()  const { return _modelDir; }
	const std::wstring& MeshStem() const { return _modelStem; }
	int  GetClipIndex() const { return _curClip; }
	void SetClipIndex(int i);           // 즉시 전환(블렌드 없음 — 직렬화 복원용)
	void Play(int clip, float blend = 0.15f); // 크로스페이드 재생
	float GetSpeed() const { return _speed; }
	void  SetSpeed(float s) { _speed = s; }
	bool  IsPlaying() const { return _playing; }
	void  SetPlaying(bool p) { _playing = p; }
	int   ClipCount() const { return (int)_clips.size(); }
	const std::vector<std::wstring>& ClipPaths() const { return _clips; }
	// 디버그 스켈레톤 — 현재 포즈의 본 부모→자식 월드 선분(에디터 "Show Bones" 오버레이용)
	void GetBoneLines(std::vector<std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3>>& out);

	// ── 상태머신 / 파라미터 (게임 코드·스크립트에서 제어) ──
	void  SetBool(const std::string& n, bool v) { _params[n] = v ? 1.f : 0.f; }
	void  SetFloat(const std::string& n, float v) { _params[n] = v; }
	void  SetTrigger(const std::string& n) { _params[n] = 1.f; } // 1프레임 소비
	bool  UseStateMachine() const { return _useSM; }
	void  SetUseStateMachine(bool b) { _useSM = b; if (b) _curState = -1; }
	void  SetupLocomotion(); // Idle/Run 상태 + Speed 파라미터 + 전이 자동 구성 (게임 코드/인스펙터 공용)
	const std::vector<std::string>& RecentEvents() const { return _eventLog; }
	std::function<void(const std::string&)> OnNotify; // 게임 콜백 (옵션)

	// 머티리얼 오버라이드 (서브메시 슬롯별 — 텍스처는 .mmat 슬롯 유지, PBR/틴트는 이 머티리얼)
	void EnsureSlotMats();                                  // _slotMats 를 max(1,_matCount) 로 채움(기본 머티리얼)
	uint32 MaterialSlotCount() const { return _matCount ? _matCount : 1; }
	shared_ptr<Material> SlotMaterial(uint32 i) { EnsureSlotMats(); return i < _slotMats.size() ? _slotMats[i] : _slotMats[0]; }
	void SetSlotMaterial(uint32 i, shared_ptr<Material> m) { if (m) { EnsureSlotMats(); if (i < _slotMats.size()) _slotMats[i] = m; } }
	// 하위호환 (슬롯 0)
	Material& GetMaterial() { EnsureSlotMats(); return *_slotMats[0]; }
	shared_ptr<Material> GetMaterialRef() const { return _slotMats.empty() ? nullptr : _slotMats[0]; }
	void SetMaterialRef(shared_ptr<Material> m) { SetSlotMaterial(0, m); }

	// 상태머신/노티파이 편집 핸들 (인스펙터 전용)
	std::vector<AnimState>&      States() { return _states; }
	std::vector<AnimTransition>& Transitions() { return _transitions; }
	std::vector<AnimNotify>&     Notifies(int clip) { EnsureNotifies(); return _notifies[clip < (int)_notifies.size() ? clip : 0]; }
	std::unordered_map<std::string, float>& Params() { return _params; }
	int  CurState() const { return _curState; }
	void SetState(int s, float blend = 0.2f); // 상태머신 상태 진입(크로스페이드)

private:
	void ScanClips();
	void CreateMaterials();  // .mmat → 슬롯별 디퓨즈/노멀/스펙 SRV 힙

	// 애니메이션 코어 (포즈 기반)
	const AnimClip* ClipData(int i);    // 지연 로드된 클립 데이터
	float ClipDuration(int i);
	void  SamplePose(const AnimClip& clip, float timeSec, bool loop, std::vector<BonePose>& out);
	void  BlendPose(std::vector<BonePose>& a, const std::vector<BonePose>& b, float w); // a = lerp(a,b,w)
	void  BuildSkinMatrices(const std::vector<BonePose>& local, std::vector<DirectX::XMMATRIX>& skinOut);
	void  ComputeAndUpload();           // 포즈 블렌드 → 본행렬/SkinParams 업로드 + AABB (GPU 디스패치는 RecordSkinning)
	void  EvalStateMachine();
	void  FireNotifies(int clip, float prevNorm, float curNorm);
	void  EnsureNotifies() { if ((int)_notifies.size() < (int)_clips.size()) _notifies.resize(_clips.size()); }

	D3D12Device* _dev = nullptr;

	// 로드 데이터
	std::vector<LoadedBone> _bonesData;
	std::vector<DirectX::XMMATRIX> _invBind; // 역바인드 행렬 캐시 (로드 1회 — 매프레임 inverse 제거)
	std::vector<SkinVtx>    _skinSrc;
	std::vector<uint32>     _indices;
	std::vector<SubMesh>    _submeshes;
	std::vector<std::wstring> _clips;        // 클립 파일 경로
	std::vector<AnimClip>     _clipData;     // 지연 로드 캐시 (_clips 와 동일 인덱스)
	// 매프레임 재사용 스크래치 (힙 할당 제거)
	std::vector<BonePose>           _poseA, _poseB;
	std::vector<DirectX::XMMATRIX>  _global, _skin;
	float                   _modelScale = 1.f;
	DirectX::XMFLOAT3       _modelOffset{ 0,0,0 };
	std::wstring            _modelDir, _modelStem;

	// 재생 / 크로스페이드 상태
	int   _curClip = 0;
	float _animTime = 0.f;
	int   _prevClip = -1;                    // 페이드아웃 중인 클립 (-1=없음)
	float _prevTime = 0.f;
	float _fadeElapsed = 0.f, _fadeDur = 0.f;
	float _blendDefault = 0.18f;             // 인스펙터 클립 전환 기본 블렌드
	float _speed = 1.f;
	bool  _playing = true;
	bool  _loop = true;

	// 상태머신
	std::vector<AnimState>      _states;
	std::vector<AnimTransition> _transitions;
	int  _curState = -1;
	bool _useSM = false;
	std::unordered_map<std::string, float> _params;

	// 노티파이
	std::vector<std::vector<AnimNotify>> _notifies; // 클립별
	std::vector<std::string> _eventLog;             // 최근 발생 이벤트(인스펙터)
	float _prevNorm = 0.f;

	// GPU 스키닝 버퍼
	std::vector<Vtx>         _world;       // (미사용 — GPU 스키닝 후 CPU 미러 없음. 인터페이스 호환 유지)
	ComPtr<ID3D12Resource>   _vb;          // 출력 월드 VB (DEFAULT+UAV — 컴퓨트가 기록, 래스터/BLAS/집계가 읽음)
	ComPtr<ID3D12Resource>   _vbPrev;      // 직전 프레임 월드 VB (속도 G버퍼 — 매프레임 _vb 복사)
	D3D12_RESOURCE_STATES    _vbPrevState = D3D12_RESOURCE_STATE_COMMON;
	bool                     _vbPrevInit = false;
	ComPtr<ID3D12Resource>   _ib;          // 인덱스 (업로드, 정적)
	ComPtr<ID3D12Resource>   _srcVB;       // 소스(바인드포즈) 정점 (업로드 SRV — 동일 메시 공유)
	ComPtr<ID3D12Resource>   _boneBuf;     // 본 스킨 행렬 (업로드 SRV, 매프레임 갱신)
	void*                    _boneMapped = nullptr; uint32 _boneCap = 0;
	ComPtr<ID3D12Resource>   _skinCB;      // SkinParams (업로드 CBV)
	void*                    _skinCBMapped = nullptr;
	D3D12_RESOURCE_STATES    _vbState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	bool                     _skinDirty = true;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};
	uint32                   _vtxCount = 0, _idxCount = 0;
	DirectX::XMFLOAT3        _aabbMin{}, _aabbMax{};
	DirectX::XMFLOAT3        _localMin{}, _localMax{}; // 바인드포즈 로컬 AABB (월드 AABB 근사용)

	// 머티리얼 (슬롯×3 디퓨즈/노멀/스펙 — 모델 셰이더 t2/t3/t4)
	std::vector<ComPtr<ID3D12Resource>> _matResources;
	ComPtr<ID3D12Resource>              _whiteTex;
	ComPtr<ID3D12Resource>              _flatNormalTex; // 노멀맵 없는 슬롯 폴백 (128,128,255=(0,0,1))
	ComPtr<ID3D12DescriptorHeap>        _srvHeap;
	UINT                                _srvInc = 0;
	uint32                              _matCount = 0;
	std::vector<uint32>                 _subMatSlot;
	bool                                _hasTexture = false;
	std::vector<shared_ptr<Material>>   _slotMats; // 머티리얼 슬롯별 PBR/틴트 오버라이드 (Element 0..N)

	ComPtr<ID3D12Resource>              _blas, _blasScratch; // RT 통합 TLAS 인스턴스용
	bool                                _blasDirty = true;   // 스키닝 변경 시에만 BLAS 재빌드
	bool                                _blasBuilt = false;  // 초기 풀빌드 완료(이후 refit)
	uint32                              _bakedVer = 0;       // 트랜스폼 더티 체크
	bool                                _skinnedOnce = false;
};
