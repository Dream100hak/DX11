#include "Mesh.h"

// 업로드 힙 버퍼 (정적 메시는 한 번 채우고 유지). 데모/소규모용 — 대형은 DEFAULT+업로드 복사 권장.
static ComPtr<ID3D12Resource> MakeUpload(ID3D12Device* dev, const void* data, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "Mesh buffer");
	if (data)
	{
		void* p = nullptr; D3D12_RANGE nr{ 0, 0 };
		buf->Map(0, &nr, &p); memcpy(p, data, size); buf->Unmap(0, nullptr);
	}
	return buf;
}

void Mesh::Create(ID3D12Device* device, const vector<Vtx>& verts, const vector<uint32>& indices)
{
	const size_t vbSize = verts.size() * sizeof(Vtx);
	const size_t ibSize = indices.size() * sizeof(uint32);

	_vb = MakeUpload(device, verts.data(), vbSize);
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbv.StrideInBytes = sizeof(Vtx);
	_vbv.SizeInBytes = (UINT)vbSize;

	_ib = MakeUpload(device, indices.data(), ibSize);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibv.Format = DXGI_FORMAT_R32_UINT;
	_ibv.SizeInBytes = (UINT)ibSize;

	_indexCount = (uint32)indices.size();
}
