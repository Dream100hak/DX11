#include "Foliage.h"
#include "D3D12Device.h"
#include "Terrain.h"
#include <cmath>

using namespace DirectX;

static ComPtr<ID3D12Resource> MakeUploadBuf(ID3D12Device* dev, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size ? size : 16; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "Foliage buffer");
	return buf;
}

namespace
{
	// 결정적 난수 (LCG)
	struct Rng { uint32 s; float f() { s = s * 1664525u + 1013904223u; return (s >> 8) * (1.0f / 16777216.0f); } };

	void AddTri(vector<Vtx>& v, vector<uint32>& idx, const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& col)
	{
		XMVECTOR pa = XMLoadFloat3(&a), pb = XMLoadFloat3(&b), pc = XMLoadFloat3(&c);
		XMVECTOR n = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa)));
		XMFLOAT3 nf; XMStoreFloat3(&nf, n);
		uint32 base = (uint32)v.size();
		Vtx va{ a, nf, col, {0,1}, {1,0,0} }, vb{ b, nf, col, {1,1}, {1,0,0} }, vc{ c, nf, col, {0.5f,0}, {1,0,0} };
		v.push_back(va); v.push_back(vb); v.push_back(vc);
		idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
	}
}

void Foliage::Generate(Terrain* terrain, int grassCount, int treeCount, float grassSize, uint32 seed)
{
	if (!_dev || !terrain) return;
	_grassCount = grassCount; _treeCount = treeCount; _grassSize = grassSize; _seed = seed;
	_verts.clear(); _indices.clear();

	const float half = terrain->HalfSize();
	Rng rng{ seed ? seed : 1u };

	XMFLOAT3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
	auto acc = [&](const XMFLOAT3& p) { mn.x = min(mn.x, p.x); mn.y = min(mn.y, p.y); mn.z = min(mn.z, p.z); mx.x = max(mx.x, p.x); mx.y = max(mx.y, p.y); mx.z = max(mx.z, p.z); };

	// ── 잔디: 교차 블레이드 2장(끝이 좁아지는 삼각형), 약간 휜 형태 ──
	for (int i = 0; i < grassCount; ++i)
	{
		float wx = (rng.f() * 2.f - 1.f) * half * 0.98f;
		float wz = (rng.f() * 2.f - 1.f) * half * 0.98f;
		float baseY = terrain->GetHeight(wx, wz);
		float h = grassSize * (0.7f + rng.f() * 0.8f);
		float w = grassSize * 0.18f;
		float bend = (rng.f() * 2.f - 1.f) * grassSize * 0.4f;
		float bz = (rng.f() * 2.f - 1.f) * grassSize * 0.4f;
		// 색: 밑동 어두운 녹색 → 끝 밝은 황록 (틴트는 흰색이라 정점색 그대로)
		XMFLOAT3 cbot(0.12f, 0.22f, 0.07f), ctop(0.40f, 0.55f, 0.18f);
		XMFLOAT3 tip{ wx + bend, baseY + h, wz + bz };
		// 블레이드 1 (X축 폭)
		AddTri(_verts, _indices, { wx - w, baseY, wz }, { wx + w, baseY, wz }, tip, ctop);
		_verts[_verts.size() - 3].col = cbot; _verts[_verts.size() - 2].col = cbot; // 밑동 두 점 어둡게
		// 블레이드 2 (Z축 폭, 교차)
		AddTri(_verts, _indices, { wx, baseY, wz - w }, { wx, baseY, wz + w }, tip, ctop);
		_verts[_verts.size() - 3].col = cbot; _verts[_verts.size() - 2].col = cbot;
		acc({ wx - w, baseY, wz }); acc(tip);
	}

	// ── 나무: 줄기(가는 사각기둥) + 잎(원뿔) 저폴리 ──
	for (int i = 0; i < treeCount; ++i)
	{
		float wx = (rng.f() * 2.f - 1.f) * half * 0.95f;
		float wz = (rng.f() * 2.f - 1.f) * half * 0.95f;
		float baseY = terrain->GetHeight(wx, wz);
		float scale = 1.0f + rng.f() * 1.5f;
		float trunkH = 1.2f * scale, trunkR = 0.08f * scale;
		float leafH = 2.0f * scale, leafR = 0.7f * scale;
		XMFLOAT3 bark(0.30f, 0.20f, 0.10f), leaf(0.13f, 0.35f, 0.12f);

		// 줄기: 사각기둥 4면
		float ty = baseY + trunkH;
		XMFLOAT3 c0{ wx - trunkR, baseY, wz - trunkR }, c1{ wx + trunkR, baseY, wz - trunkR };
		XMFLOAT3 c2{ wx + trunkR, baseY, wz + trunkR }, c3{ wx - trunkR, baseY, wz + trunkR };
		XMFLOAT3 t0{ c0.x, ty, c0.z }, t1{ c1.x, ty, c1.z }, t2{ c2.x, ty, c2.z }, t3{ c3.x, ty, c3.z };
		AddTri(_verts, _indices, c0, c1, t1, bark); AddTri(_verts, _indices, c0, t1, t0, bark);
		AddTri(_verts, _indices, c1, c2, t2, bark); AddTri(_verts, _indices, c1, t2, t1, bark);
		AddTri(_verts, _indices, c2, c3, t3, bark); AddTri(_verts, _indices, c2, t3, t2, bark);
		AddTri(_verts, _indices, c3, c0, t0, bark); AddTri(_verts, _indices, c3, t0, t3, bark);

		// 잎: 원뿔(밑면 6각 → 꼭대기)
		float leafBaseY = baseY + trunkH * 0.7f;
		XMFLOAT3 apex{ wx, leafBaseY + leafH, wz };
		const int seg = 6;
		for (int s = 0; s < seg; ++s)
		{
			float a0 = s / (float)seg * 6.2831853f, a1 = (s + 1) / (float)seg * 6.2831853f;
			XMFLOAT3 p0{ wx + cosf(a0) * leafR, leafBaseY, wz + sinf(a0) * leafR };
			XMFLOAT3 p1{ wx + cosf(a1) * leafR, leafBaseY, wz + sinf(a1) * leafR };
			AddTri(_verts, _indices, p0, p1, apex, leaf);
		}
		acc({ wx - leafR, baseY, wz - leafR }); acc({ wx + leafR, apex.y, wz + leafR });
	}

	if (_verts.empty()) { acc({ 0,0,0 }); acc({ 1,1,1 }); }
	_aabbMin = mn; _aabbMax = mx;
	Upload();
}

void Foliage::Upload()
{
	if (!_dev || _verts.empty()) return;
	const size_t vbSize = _verts.size() * sizeof(Vtx);
	const size_t ibSize = _indices.size() * sizeof(uint32);
	_vb = MakeUploadBuf(_dev->_device.Get(), vbSize);
	D3D12_RANGE nr{ 0, 0 }; _vb->Map(0, &nr, &_vbMapped); memcpy(_vbMapped, _verts.data(), vbSize);
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress(); _vbv.StrideInBytes = sizeof(Vtx); _vbv.SizeInBytes = (UINT)vbSize;
	_ib = MakeUploadBuf(_dev->_device.Get(), ibSize);
	void* p = nullptr; _ib->Map(0, &nr, &p); memcpy(p, _indices.data(), ibSize); _ib->Unmap(0, nullptr);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress(); _ibv.Format = DXGI_FORMAT_R32_UINT; _ibv.SizeInBytes = (UINT)ibSize;
}

void Foliage::TransformBoundingBox()
{
	XMVECTOR mn = XMLoadFloat3(&_aabbMin), mx = XMLoadFloat3(&_aabbMax);
	XMStoreFloat3(&_boundingBox.Center, XMVectorScale(XMVectorAdd(mn, mx), 0.5f));
	XMStoreFloat3(&_boundingBox.Extents, XMVectorScale(XMVectorSubtract(mx, mn), 0.5f));
}

void Foliage::Draw(const RenderContext& ctx)
{
	if (!_dev || _verts.empty() || !_vb) return;
	D3D12Device& d = *_dev;
	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, ctx.cb);
	cmd->SetGraphicsRootShaderResourceView(1, d._scene._tlas->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr());
	// 정점색 경로 (mode 2), 틴트 흰색
	struct { uint32 mode; float met, rough, emis, tr, tg, tb, pad; } mc{ 2u, 0.f, 0.9f, 0.f, 1.f, 1.f, 1.f, 0.f };
	cmd->SetGraphicsRoot32BitConstants(4, 8, &mc, 0);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced((UINT)_indices.size(), 1, 0, 0, 0);
}
