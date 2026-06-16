#include "GridRenderer.h"
#include "D3D12Device.h"

// 씬 그리드 (지오메트리 위, 깊이 테스트) — (D3D12Render.cpp 인라인 이관)
void GridRenderer::Draw(const RenderContext& ctx)
{
	if (!_dev) return;
	D3D12Device& d = *_dev;
	if (!d._showGrid) return;

	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._gridPSO.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, d._cb[d._frameIndex]->GetGPUVirtualAddress());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}
