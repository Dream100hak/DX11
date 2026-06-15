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

private:
	void CreateFontTexture(ID3D12CommandQueue* queue);

	ID3D12Device*                 _dev = nullptr;
	UINT                          _frames = 2;
	ComPtr<ID3D12RootSignature>   _rootSig;
	ComPtr<ID3D12PipelineState>   _pso;
	ComPtr<ID3D12Resource>        _fontTex;
	ComPtr<ID3D12DescriptorHeap>  _srvHeap;   // shader-visible, slot0 = 폰트
	D3D12_GPU_DESCRIPTOR_HANDLE   _fontGpu{};

	struct FrameBuf
	{
		ComPtr<ID3D12Resource> vb, ib;
		UINT vbCount = 0, ibCount = 0; // 현재 용량(정점/인덱스 수)
	};
	std::vector<FrameBuf>         _frameBufs;
};
