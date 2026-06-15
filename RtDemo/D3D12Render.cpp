#include "D3D12Device.h"

using namespace DirectX;

// ───────────────────────────────────────────────────────────
// 프레임 렌더 루프 — 스키닝 → AS 재빌드 → DDGI 디스패치 → 라스터(모델/바닥) → Present
// (D3D12Device.cpp 에서 분리)
// ───────────────────────────────────────────────────────────
void D3D12Device::Render()
{
	_time += 1.0f / 60.0f;

	// ── 상수버퍼 갱신 (정적 지오메트리=항등, 빛 방향 애니메이션 → RT 그림자 이동) ──
	XMMATRIX model = XMMatrixIdentity();
	XMVECTOR eye = XMVectorSet(3.4f, 2.4f, -4.6f, 1.f);
	XMVECTOR at  = XMVectorSet(0.f, 1.05f, 0.f, 1.f);
	XMVECTOR up  = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
	float aspect = float(_width) / float(_height);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(55.f), aspect, 0.1f, 100.f);

	// 빛이 머리 위를 천천히 도는 형태 (그림자/간접광이 같이 변함)
	float a = _time * 0.6f;

	SceneCB cb;
	XMStoreFloat4x4(&cb.mvp, model * view * proj); // row_major HLSL → 전치 불필요
	XMStoreFloat4x4(&cb.model, model);
	cb.lightDir = XMFLOAT4(cosf(a) * 0.6f, -1.0f, sinf(a) * 0.6f, 1.2f); // w=세기
	XMStoreFloat4(&cb.camPos, eye);
	cb.gridMin  = XMFLOAT4(-6.5f, 0.2f, -6.5f, 0.f);
	cb.gridMax  = XMFLOAT4( 6.5f, 4.0f,  6.5f, 0.f);
	cb.gridDim  = XMFLOAT4(float(PROBE_X), float(PROBE_Y), float(PROBE_Z), 128.f); // 128 rays/probe
	cb.giParams = XMFLOAT4(0.45f, _time * 60.f, 0.03f, 0.f); // GI세기(≈1/π 부근) / frame / 미세 앰비언트
	memcpy(_cbMapped[_frameIndex], &cb, sizeof(cb));

	// ── 스키닝: CPU 본 변형 → VB 갱신 (GPU 유휴 시점, 전체 플러시 동기화) ──
	if (_animated)
		UpdateAnimation();

	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), nullptr), "CmdList Reset");

	// 스키닝 시 갱신된 정점으로 BLAS/TLAS 매 프레임 재빌드
	if (_animated)
		RecordBuildAS();

	// ── DDGI: 프로브 irradiance 갱신 (컴퓨트 RT) — 라스터 전에 ──
	DispatchGI();

	D3D12_RESOURCE_BARRIER toRT{};
	toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toRT.Transition.pResource = _renderTargets[_frameIndex].Get();
	toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &toRT);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += SIZE_T(_frameIndex) * _rtvDescSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	float clear[4] = { 0.06f, 0.07f, 0.10f, 1.0f };
	_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT vp{ 0.f, 0.f, float(_width), float(_height), 0.f, 1.f };
	D3D12_RECT sc{ 0, 0, LONG(_width), LONG(_height) };
	_cmdList->RSSetViewports(1, &vp);
	_cmdList->RSSetScissorRects(1, &sc);

	_cmdList->SetPipelineState(_pso.Get()); // 그래픽스 PSO (컴퓨트 후 복귀)
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
	_cmdList->SetGraphicsRootShaderResourceView(1, _tlas->GetGPUVirtualAddress()); // TLAS (RayQuery)
	_cmdList->SetGraphicsRootShaderResourceView(2, _probes->GetGPUVirtualAddress()); // DDGI 프로브
	if (_hasTexture)
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, heaps);
	}
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, 1, &_vbv);
	_cmdList->IASetIndexBuffer(&_ibv);

	// 모델: 서브메시별 머티리얼 텍스처 테이블 오프셋 후 드로우
	if (_hasTexture && !_submeshes.empty())
	{
		_cmdList->SetGraphicsRoot32BitConstant(4, 1u, 0); // useTex
		D3D12_GPU_DESCRIPTOR_HANDLE base = _srvHeap->GetGPUDescriptorHandleForHeapStart();
		for (size_t i = 0; i < _submeshes.size(); ++i)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += SIZE_T(_subMatSlot[i]) * 3 * _srvInc; // 슬롯×3 디스크립터
			_cmdList->SetGraphicsRootDescriptorTable(3, h);
			_cmdList->DrawIndexedInstanced(_submeshes[i].indexCount, 1, _submeshes[i].indexStart, 0, 0);
		}
	}
	else
	{
		_cmdList->SetGraphicsRoot32BitConstant(4, 0u, 0);
		_cmdList->DrawIndexedInstanced(_modelIndexCount, 1, 0, 0, 0); // 모델(정점색 폴백)
	}

	// 바닥(정점색)
	_cmdList->SetGraphicsRoot32BitConstant(4, 0u, 0);
	_cmdList->DrawIndexedInstanced(_indexCount - _modelIndexCount, 1, _modelIndexCount, 0, 0);

	D3D12_RESOURCE_BARRIER toPresent = toRT;
	toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &toPresent);

	ThrowIfFailed(_cmdList->Close(), "CmdList Close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);

	ThrowIfFailed(_swapChain->Present(1, 0), "Present");
	MoveToNextFrame();
}
