#include "SkyRenderer.h"
#include "D3D12Device.h"

// 스카이박스 (배경, 깊이 없음) — 솔리드 배경 모드면 생략. (D3D12Render.cpp 인라인 이관)
void SkyRenderer::Draw(const RenderContext& ctx)
{
	if (!_dev) return;
	D3D12Device& d = *_dev;
	if (!(d._showSky && d._bgMode == 0)) return;

	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._skyPSO.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, ctx.cb); // 카메라별 CB (Scene/Game)
	// 큐브맵 스카이박스 — 큐브 SRV 테이블(t2~t4) 바인드 (셰이더가 gExtra.w 로 분기)
	if (d._skyCubemapOn && d._skyCubeHeap)
	{
		ID3D12DescriptorHeap* heaps[] = { d._skyCubeHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetGraphicsRootDescriptorTable(3, d._skyCubeHeap->GetGPUDescriptorHandleForHeapStart());
	}
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}
