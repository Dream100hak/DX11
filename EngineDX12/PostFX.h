#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// PostFX — HDR 씬 RT 후처리 체인 (블룸 → 톤맵(ACES+그레이딩+DOF+갓레이) → FXAA).
// LDR/LDR2/블룸 핑퐁 RT 와 전용 SRV/RTV 디스크립터 힙, 4개 PSO 를 소유한다.
// 씬 컬러/깊이 RT(_sceneRT/_sceneDepth)는 D3D12Device 가 소유 — Resize 에서 그 SRV만 생성한다.
// 호출 순서: Init(1회) → 창 크기 변경마다 Resize → 매 프레임 Bloom → Tonemap → Fxaa.
// 톤맵/FXAA 결과(LDR 또는 LDR2)를 ImGui "Scene" 이미지로 표시한다.
// ───────────────────────────────────────────────────────────
class PostFX
{
public:
	// 톤맵 패스 파라미터 (에디터/카메라 상태 → 셰이더 상수). D3D12Device 가 채워 전달.
	struct TonemapParams
	{
		float exposure;        // 이미 _exposure × 2^EV 로 결합된 값
		float bloomIntensity;
		bool  bloomEnabled;    // 블룸 합성 여부 (_bloomOn && Ready())
		int   tonemapOp;       // 0 ACES / 1 Reinhard / 2 Filmic
		float contrast, saturation, temperature, vignette;
		float chroma, grain, sharpen, time;
		float expScale;        // 자동 노출 스케일
		float sunSX, sunSY; bool sunVisible; // 갓레이용 태양 화면 좌표
		float volStrength, dofFocus, dofRange; bool dofOn, volOn;
		float lensDistort, posterize; bool anamorphic; int filterMode;
	};

	void Init(ID3D12Device* device, DXGI_FORMAT sceneFmt);
	// 씬 RT/깊이 크기 변경 시 LDR/LDR2/블룸 재생성 + 모든 SRV/RTV 갱신.
	// sceneRT/sceneDepth 는 소유하지 않음 — 포스트 SRV 힙에 뷰만 만든다.
	void Resize(UINT w, UINT h, ID3D12Resource* sceneRT, ID3D12Resource* sceneDepth);

	// 블룸: sceneRT(HDR, slot0) → 브라이트패스 → BlurH → BlurV → bloomA. (호출 전 sceneRT 는 PSR 상태)
	void Bloom(ID3D12GraphicsCommandList4* cmd, float threshold);
	// 톤맵: HDR + bloom + depth → LDR. (호출 전 sceneDepth 는 PSR 상태)
	void Tonemap(ID3D12GraphicsCommandList4* cmd, const TonemapParams& p);
	// FXAA: on 이면 LDR→LDR2 후 LDR2 반환, off 면 LDR 그대로 반환. (표시/리드백 대상)
	ID3D12Resource* Fxaa(ID3D12GraphicsCommandList4* cmd, bool fxaaOn);

	bool            Ready() const { return _bloomReady; }
	ID3D12Resource* LdrResource() const { return _sceneLDR.Get(); } // 스크린샷 리드백/표시

private:
	void Transition(ID3D12GraphicsCommandList4* cmd, ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to);

	ID3D12Device* _device = nullptr;
	DXGI_FORMAT   _sceneFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
	UINT          _w = 0, _h = 0;

	ComPtr<ID3D12RootSignature>  _rootSig;
	ComPtr<ID3D12PipelineState>  _tonemapPSO, _brightPSO, _blurPSO, _fxaaPSO;
	ComPtr<ID3D12DescriptorHeap> _srvHeap;   // 0 HDR씬 / 1 bloomA / 2 depth / 3 bloomB / 4 LDR / 5 LDR2
	UINT                         _srvInc = 0;
	ComPtr<ID3D12DescriptorHeap> _rtvHeap;   // 0 LDR / 1 LDR2 / 2 bloomA / 3 bloomB
	UINT                         _rtvInc = 0;

	ComPtr<ID3D12Resource>       _sceneLDR, _sceneLDR2;  // 톤맵 / FXAA 결과
	D3D12_RESOURCE_STATES        _ldrState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES        _ldr2State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	ComPtr<ID3D12Resource>       _bloomA, _bloomB;       // 반해상도 핑퐁
	D3D12_RESOURCE_STATES        _bloomAState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES        _bloomBState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	UINT                         _bloomW = 0, _bloomH = 0;
	bool                         _bloomReady = false;
};
