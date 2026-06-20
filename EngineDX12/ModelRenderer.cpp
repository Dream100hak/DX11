#include "ModelRenderer.h"
#include "D3D12Device.h"

using namespace DirectX;

// 모델 월드 AABB(ModelScene 가 스키닝 후 매 프레임 산출)로 바운딩 박스 갱신
void ModelRenderer::TransformBoundingBox()
{
	if (!_dev) return;
	const Vec3& mn = _dev->_scene._modelMin;
	const Vec3& mx = _dev->_scene._modelMax;
	_boundingBox.Center  = XMFLOAT3((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
	_boundingBox.Extents = XMFLOAT3((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f, (mx.z - mn.z) * 0.5f);
}

// D3D12Render.cpp 의 인라인 모델+바닥 불투명 드로우를 컴포넌트로 이관.
// ctx.cmd 에 기록하고, PSO/루트시그/CB/TLAS/DDGI/ModelScene 은 _dev(friend)로 접근.
void ModelRenderer::Draw(const RenderContext& ctx)
{
	if (!_dev) return;
	D3D12Device& d = *_dev;
	if (!d._showFloor) return; // 바닥 숨김 토글
	auto* cmd = ctx.cmd;
	ModelScene& sc = d._scene;

	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get()); // 그래픽스 PSO (컴퓨트 후 복귀)
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, ctx.cb); // 카메라별 CB (Scene/Game)
	cmd->SetGraphicsRootShaderResourceView(1, sc._tlas->GetGPUVirtualAddress()); // TLAS (RayQuery)
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());     // DDGI 프로브
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr()); // 프로브 depth
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &sc._vbv);
	cmd->IASetIndexBuffer(&sc._ibv);

	// 바닥만 (모델은 ModelAnimator GameObject 로 분리됨 — _floorOnly). mode 0 = gFloorMat.
	struct MatC { uint32 mode; float met, rough, emis, tr, tg, tb, pad; };
	MatC floor{ 0u, 0,0,0,0,0,0,0 };
	cmd->SetGraphicsRoot32BitConstants(4, 8, &floor, 0);
	cmd->DrawIndexedInstanced(sc._indexCount - sc._modelIndexCount, 1, sc._modelIndexCount, 0, 0);
}
