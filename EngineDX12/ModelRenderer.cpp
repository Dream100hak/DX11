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
	auto* cmd = ctx.cmd;
	D3D12Device& d = *_dev;
	ModelScene& sc = d._scene;

	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get()); // 그래픽스 PSO (컴퓨트 후 복귀)
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, d._cb[d._frameIndex]->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(1, sc._tlas->GetGPUVirtualAddress()); // TLAS (RayQuery)
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());     // DDGI 프로브
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr()); // 프로브 depth
	if (sc._hasTexture)
	{
		ID3D12DescriptorHeap* heaps[] = { sc._srvHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
	}
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &sc._vbv);
	cmd->IASetIndexBuffer(&sc._ibv);

	// 모델: 서브메시별 머티리얼 텍스처 테이블 오프셋 후 드로우
	if (sc._hasTexture && !sc._submeshes.empty())
	{
		cmd->SetGraphicsRoot32BitConstant(4, 1u, 0); // useTex
		D3D12_GPU_DESCRIPTOR_HANDLE base = sc._srvHeap->GetGPUDescriptorHandleForHeapStart();
		for (size_t i = 0; i < sc._submeshes.size(); ++i)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += SIZE_T(sc._subMatSlot[i]) * 3 * sc._srvInc; // 슬롯×3 디스크립터
			cmd->SetGraphicsRootDescriptorTable(3, h);
			cmd->DrawIndexedInstanced(sc._submeshes[i].indexCount, 1, sc._submeshes[i].indexStart, 0, 0);
		}
	}
	else
	{
		cmd->SetGraphicsRoot32BitConstant(4, 2u, 0); // 모델 정점색 폴백
		cmd->DrawIndexedInstanced(sc._modelIndexCount, 1, 0, 0, 0);
	}

	// 바닥(정점색)
	cmd->SetGraphicsRoot32BitConstant(4, 0u, 0);
	cmd->DrawIndexedInstanced(sc._indexCount - sc._modelIndexCount, 1, sc._modelIndexCount, 0, 0);
}
