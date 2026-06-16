#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// DebugDraw — 저수준 디버그 라인 렌더러.
// 본 스켈레톤 / AABB / 라이트 아이콘 / 스팟 콘 / 파티클 시각화의 GPU 플러밍만 담당.
// 깊이 ON 배치(메시에 가려짐)와 오버레이 배치(깊이 OFF — 메시 관통, 스켈레톤용) 2종.
// "무엇을 그릴지"(에디터 상태 → 라인)는 호출측(D3D12Device::DrawDebugLines)이 결정한다.
// 공유 루트시그(b0=SceneCB, gMVP) + 씬 RT 포맷을 받아 PSO 2개를 생성한다.
// ───────────────────────────────────────────────────────────
class DebugDraw
{
public:
	void Init(ID3D12Device* device, ID3D12RootSignature* rootSig, DXGI_FORMAT sceneFmt);

	void Begin(); // 프레임 시작 — 배치 클리어

	// 라인 1개 추가. overlay=true → 깊이 무시 배치(스켈레톤처럼 메시 관통 표시)
	void Line(DirectX::XMFLOAT3 a, DirectX::XMFLOAT3 b, DirectX::XMFLOAT3 col, bool overlay = false);
	// 위치 p 에 3축 크로스(반경 s) 추가
	void Cross(DirectX::XMFLOAT3 p, DirectX::XMFLOAT3 col, float s, bool overlay = false);

	// 업로드 + 드로우 (b0 = cbAddr 의 SceneCB 를 바인드, gMVP 사용)
	void Flush(ID3D12GraphicsCommandList* cmd, D3D12_GPU_VIRTUAL_ADDRESS cbAddr);

private:
	struct Batch
	{
		std::vector<float>     data;            // pos(3)+col(3) 인터리브
		ComPtr<ID3D12Resource> vb;
		void*                  mapped = nullptr;
		UINT                   cap = 0;         // 바이트
	};
	void DrawBatch(ID3D12GraphicsCommandList* cmd, Batch& b, ID3D12PipelineState* pso);

	ID3D12Device*               _device = nullptr;
	ID3D12RootSignature*        _rootSig = nullptr;
	ComPtr<ID3D12PipelineState> _pso;        // 깊이 ON (LESS_EQUAL, 쓰기 없음)
	ComPtr<ID3D12PipelineState> _overlayPSO; // 깊이 OFF
	Batch                       _depth;      // 깊이 테스트 라인
	Batch                       _overlay;    // 오버레이 라인(스켈레톤)
};
