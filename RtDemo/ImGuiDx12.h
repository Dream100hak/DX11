#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// 미니멀 ImGui DX12 렌더 백엔드 (공식 imgui_impl_dx12 가 없어 직접 작성).
// 폰트 아틀라스 텍스처 + 2D 텍스처 PSO + 프레임별 동적 VB/IB.
// 입력은 imgui_impl_win32 재사용, 코어는 Engine/imgui*.cpp 공용.
// ───────────────────────────────────────────────────────────
class ImGuiDx12
{
public:
	void Init(ID3D12Device* dev, ID3D12CommandQueue* queue, DXGI_FORMAT rtvFormat, UINT framesInFlight);
	void Render(ID3D12GraphicsCommandList* cmd, UINT frameIndex); // ImGui::Render() 후 호출
	void Shutdown();

	// 씬 RT 텍스처를 슬롯1 SRV 로 등록(리사이즈 시 재호출) → ImGui::Image 용 ImTextureID 반환
	uint64 SetSceneTexture(ID3D12Resource* tex);

private:
	void CreateFontTexture(ID3D12CommandQueue* queue);

	ID3D12Device*                 _dev = nullptr;
	UINT                          _frames = 2;
	ComPtr<ID3D12RootSignature>   _rootSig;
	ComPtr<ID3D12PipelineState>   _pso;
	ComPtr<ID3D12Resource>        _fontTex;
	ComPtr<ID3D12DescriptorHeap>  _srvHeap;   // shader-visible, slot0 = 폰트, slot1 = 씬RT
	D3D12_GPU_DESCRIPTOR_HANDLE   _fontGpu{};
	UINT                          _srvInc = 0;

	struct FrameBuf
	{
		ComPtr<ID3D12Resource> vb, ib;
		UINT vbCount = 0, ibCount = 0; // 현재 용량(정점/인덱스 수)
	};
	std::vector<FrameBuf>         _frameBufs;
};
