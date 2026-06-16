#include "Ddgi.h"

using namespace DirectX;

ComPtr<ID3D12Resource> Ddgi::DefaultBufferUAV(UINT64 size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buf)), "Ddgi DefaultBufferUAV");
	return buf;
}

void Ddgi::Create(ID3D12Device* device, const void* gatherCS, size_t gatherCSSize)
{
	_device = device;

	// 프로브 버퍼 (ProbeSH = float3 × 4 = SH-L1) × PROBE_COUNT — 컴퓨트 UAV 쓰기 / 픽셀 SRV 읽기
	_probes = DefaultBufferUAV(PROBE_COUNT * sizeof(XMFLOAT3) * 4);
	_probeState = D3D12_RESOURCE_STATE_COMMON;

	// 옥타헤드럴 depth 버퍼 (float2 mean/mean²) × PROBE_COUNT × OCT²
	_probeDepth = DefaultBufferUAV(PROBE_COUNT * PROBE_OCT * PROBE_OCT * sizeof(XMFLOAT2));
	_probeDepthState = D3D12_RESOURCE_STATE_COMMON;

	// 컴퓨트 루트 시그니처: b0 CBV, u0 probes, u1 depth, t0 TLAS, t1 verts, t2 indices (전부 루트 디스크립터)
	D3D12_ROOT_PARAMETER p[6]{};
	p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; p[0].Descriptor.ShaderRegister = 0;
	p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[1].Descriptor.ShaderRegister = 0;
	p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[2].Descriptor.ShaderRegister = 1; // depth
	p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[3].Descriptor.ShaderRegister = 0;
	p[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[4].Descriptor.ShaderRegister = 1;
	p[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[5].Descriptor.ShaderRegister = 2;
	for (auto& rp : p) rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 6;
	rs.pParameters = p;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "Serialize GI RootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "Create GI RootSig");

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig.Get();
	pso.CS = { gatherCS, gatherCSSize };
	ThrowIfFailed(_device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&_pso)), "Create GI PSO");
}

void Ddgi::Transition(ID3D12GraphicsCommandList4* cmd, ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to)
{
	if (cur == to) return;
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.StateBefore = cur;
	b.Transition.StateAfter = to;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd->ResourceBarrier(1, &b);
	cur = to;
}

void Ddgi::Dispatch(ID3D12GraphicsCommandList4* cmd,
                    D3D12_GPU_VIRTUAL_ADDRESS cb,
                    D3D12_GPU_VIRTUAL_ADDRESS tlas,
                    D3D12_GPU_VIRTUAL_ADDRESS vb,
                    D3D12_GPU_VIRTUAL_ADDRESS ib)
{
	// 프로브/depth 를 UAV 상태로 → RT 레이 수집 → 픽셀 읽기용 SRV 로 전환
	Transition(cmd, _probes.Get(), _probeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cmd, _probeDepth.Get(), _probeDepthState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmd->SetPipelineState(_pso.Get());
	cmd->SetComputeRootSignature(_rootSig.Get());
	cmd->SetComputeRootConstantBufferView(0, cb);
	cmd->SetComputeRootUnorderedAccessView(1, _probes->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(2, _probeDepth->GetGPUVirtualAddress());
	cmd->SetComputeRootShaderResourceView(3, tlas);
	cmd->SetComputeRootShaderResourceView(4, vb);
	cmd->SetComputeRootShaderResourceView(5, ib);
	cmd->Dispatch((PROBE_COUNT + 63) / 64, 1, 1);

	D3D12_RESOURCE_BARRIER uav[2]{};
	uav[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav[0].UAV.pResource = _probes.Get();
	uav[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav[1].UAV.pResource = _probeDepth.Get();
	cmd->ResourceBarrier(2, uav);

	Transition(cmd, _probes.Get(), _probeState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Transition(cmd, _probeDepth.Get(), _probeDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
