#pragma once
#include "Common.h"

// DX11 Engine/Mesh 이식(DX12) — GPU 정점/인덱스 버퍼 리소스. (정적 메시 MeshRenderer 용)
class Mesh
{
public:
	void Create(ID3D12Device* device, const vector<Vtx>& verts, const vector<uint32>& indices);

	const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return _vbv; }
	const D3D12_INDEX_BUFFER_VIEW&  GetIBV() const { return _ibv; }
	uint32 GetIndexCount() const { return _indexCount; }

private:
	ComPtr<ID3D12Resource>   _vb, _ib;
	D3D12_VERTEX_BUFFER_VIEW _vbv{};
	D3D12_INDEX_BUFFER_VIEW  _ibv{};
	uint32                   _indexCount = 0;
};
