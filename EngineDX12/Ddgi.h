#pragma once
#include "Common.h"

// ───────────────────────────────────────────────────────────
// Ddgi — Dynamic Diffuse Global Illumination 프로브 볼륨.
// 프로브 SH irradiance + 옥타헤드럴 depth 버퍼를 소유하고, 매 프레임 컴퓨트
// 셰이더로 하드웨어 레이트레이싱(RayQuery) 1바운스 수집을 디스패치한다.
// 가속구조(BLAS/TLAS)는 모델 지오메트리에 묶여 있어 D3D12Device 가 소유 —
// Dispatch 시 TLAS/정점/인덱스 GPU 주소를 받아 바인드한다.
// 컴퓨트 셰이더(kGatherShader)는 공용 SceneCB 레이아웃에 의존하므로
// 컴파일은 D3D12Device 가 하고, 바이트코드만 Create 로 전달받는다.
// ───────────────────────────────────────────────────────────
class Ddgi
{
public:
	// 프로브 격자 (gather 셰이더의 grid/OCT 상수와 반드시 일치)
	static const UINT PROBE_X = 10;
	static const UINT PROBE_Y = 5;
	static const UINT PROBE_Z = 10;
	static const UINT PROBE_COUNT = PROBE_X * PROBE_Y * PROBE_Z;
	static const UINT PROBE_OCT = 8; // 프로브당 옥타헤드럴 depth 해상도

	void Create(ID3D12Device* device, const void* gatherCS, size_t gatherCSSize);

	// 매 프레임 프로브 irradiance 갱신 (RT 레이 수집). 결과는 픽셀 SRV 상태로 남긴다.
	void Dispatch(ID3D12GraphicsCommandList4* cmd,
	              D3D12_GPU_VIRTUAL_ADDRESS cb,
	              D3D12_GPU_VIRTUAL_ADDRESS tlas,
	              D3D12_GPU_VIRTUAL_ADDRESS vb,
	              D3D12_GPU_VIRTUAL_ADDRESS ib);

	D3D12_GPU_VIRTUAL_ADDRESS ProbesAddr() const { return _probes->GetGPUVirtualAddress(); }
	D3D12_GPU_VIRTUAL_ADDRESS ProbeDepthAddr() const { return _probeDepth->GetGPUVirtualAddress(); }

private:
	ComPtr<ID3D12Resource> DefaultBufferUAV(UINT64 size);
	void Transition(ID3D12GraphicsCommandList4* cmd, ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to);

	ID3D12Device*               _device = nullptr;
	ComPtr<ID3D12RootSignature> _rootSig;
	ComPtr<ID3D12PipelineState> _pso;
	ComPtr<ID3D12Resource>      _probes;      // ProbeSH[PROBE_COUNT] (UAV/SRV)
	ComPtr<ID3D12Resource>      _probeDepth;  // float2[PROBE_COUNT×OCT²] mean/mean² (UAV/SRV)
	D3D12_RESOURCE_STATES       _probeState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES       _probeDepthState = D3D12_RESOURCE_STATE_COMMON;
};
