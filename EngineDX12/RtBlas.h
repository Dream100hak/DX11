#pragma once
#include "Common.h"

// per-renderer BLAS 빌드 헬퍼 (MeshRenderer/ModelAnimator 공용 — 통합 TLAS 인스턴스용).
// 디바이스 private 우회: CreateCommittedResource 로 AS/스크래치 버퍼 직접 생성.
namespace RtBlas
{
	inline ComPtr<ID3D12Resource> MakeASBuffer(ID3D12Device5* dev, UINT64 size, D3D12_RESOURCE_STATES state)
	{
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		ComPtr<ID3D12Resource> b;
		dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, nullptr, IID_PPV_ARGS(&b));
		return b;
	}

	// 월드 VB/IB 로 BLAS 빌드. 동적 지오메트리(스키닝)는 allowUpdate=true 로 첫 프레임만 풀 빌드,
	// 이후 매 프레임 in-place refit(PERFORM_UPDATE) — 풀 리빌드보다 수배 저렴. built 는 초기빌드 여부 추적.
	inline void Build(ID3D12Device5* dev, ID3D12GraphicsCommandList4* cmd,
	                  ID3D12Resource* vb, ID3D12Resource* ib, UINT vcount, UINT icount, UINT stride,
	                  ComPtr<ID3D12Resource>& blas, ComPtr<ID3D12Resource>& scratch,
	                  bool& built, bool allowUpdate = false)
	{
		if (!vb || !ib || vcount == 0 || icount == 0) return;

		D3D12_RAYTRACING_GEOMETRY_DESC geom{};
		geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geom.Triangles.VertexBuffer.StartAddress = vb->GetGPUVirtualAddress();
		geom.Triangles.VertexBuffer.StrideInBytes = stride;
		geom.Triangles.VertexCount = vcount;
		geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geom.Triangles.IndexBuffer = ib->GetGPUVirtualAddress();
		geom.Triangles.IndexCount = icount;
		geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

		const bool doUpdate = allowUpdate && built && blas;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS in{};
		in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		in.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		in.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
		if (allowUpdate) in.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		if (doUpdate)    in.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		in.NumDescs = 1;
		in.pGeometryDescs = &geom;

		if (!blas)
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
			dev->GetRaytracingAccelerationStructurePrebuildInfo(&in, &info);
			blas    = MakeASBuffer(dev, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
			// 스크래치는 빌드/업데이트 중 큰 쪽 (업데이트 스크래치는 보통 더 작음)
			UINT64 scr = max(info.ScratchDataSizeInBytes, info.UpdateScratchDataSizeInBytes);
			scratch = MakeASBuffer(dev, scr, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
		build.Inputs = in;
		build.DestAccelerationStructureData = blas->GetGPUVirtualAddress();
		build.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
		if (doUpdate) build.SourceAccelerationStructureData = blas->GetGPUVirtualAddress(); // in-place refit
		cmd->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
		built = true;
	}

	// 단위행렬 인스턴스 desc (월드 베이크 지오메트리 — 변환은 이미 정점에 적용됨)
	inline D3D12_RAYTRACING_INSTANCE_DESC IdentityInstance(D3D12_GPU_VIRTUAL_ADDRESS blasAddr)
	{
		D3D12_RAYTRACING_INSTANCE_DESC d{};
		d.Transform[0][0] = 1.f; d.Transform[1][1] = 1.f; d.Transform[2][2] = 1.f;
		d.InstanceMask = 0xFF; d.AccelerationStructure = blasAddr;
		return d;
	}
}
