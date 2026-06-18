#include "D3D12Device.h"
#include "Renderer.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Terrain.h"
#include "Billboard.h"
#include "ParticleSystem.h"
#include "RtBlas.h"
#include "Camera.h"
#include "Light.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "imgui.h"

using namespace DirectX;

// ───────────────────────────────────────────────────────────
// 특수 렌더 패스 — 물 평면 / 테셀레이션 터레인 / 빌보드 파티클
// (D3D12Render.cpp 에서 분리, 전부 D3D12Device 메서드)
// ───────────────────────────────────────────────────────────
// 물 평면 — 절차적 그리드(VS) + 사인파 변위 + 프레넬 하늘반사. 반투명. ctx.cb=b0.
void D3D12Device::RenderWater(const RenderContext& ctx)
{
	if (!_waterOn || !_waterPSO) return;
	float wcb[8] = { _waterLevel, _waterSize, _waterGrid, _time, 0, 0, 0, 0 }; // level/size/grid/time
	UINT G = (UINT)_waterGrid;
	_cmdList->SetPipelineState(_waterPSO.Get());
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, ctx.cb);   // b0 VP/cam/sky/sun
	_cmdList->SetGraphicsRoot32BitConstants(4, 8, wcb, 0);    // b1 water params
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->DrawInstanced(G * G * 6, 1, 0, 0);              // 절차 그리드(VB 없음)
}

// 선택/첫 Terrain 을 GPU 테셀레이션(코어스 패치+HS/DS 변위)으로 렌더. 메인 루트시그 재사용.
void D3D12Device::RenderTessTerrain(const RenderContext& ctx)
{
	using namespace DirectX;
	if (!_tessTerrain || !_tessPSO || !_gameScene) return;

	shared_ptr<Terrain> terr;
	if (_selectedGO) terr = _selectedGO->GetTerrain();
	if (!terr) for (auto& kv : _gameScene->GetCreatedObjects()) { if (kv.second) if (auto t = kv.second->GetTerrain()) { terr = t; break; } }
	if (!terr) return;

	const int N = terr->GridN();
	const float cell = terr->CellSize(), half = terr->HalfSize(), worldSize = N * cell;
	const auto& heights = terr->Heightmap();
	if (heights.empty()) return;

	auto ensure = [&](ComPtr<ID3D12Resource>& buf, void*& mapped, UINT& cap, UINT bytes)
	{
		if (cap >= bytes) return;
		buf.Reset();
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = bytes + 4096; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "Tess buffer");
		cap = bytes + 4096; D3D12_RANGE nr{ 0, 0 }; buf->Map(0, &nr, &mapped);
	};

	// 하이트맵 StructuredBuffer 업로드
	UINT hBytes = (UINT)(heights.size() * sizeof(float));
	ensure(_tessHeights, _tessHeightsMapped, _tessHeightsCap, hBytes);
	memcpy(_tessHeightsMapped, heights.data(), hBytes);

	// 코어스 쿼드 패치 컨트롤포인트 (P×P 패치, 패치당 4CP). uv[0,1], world XZ 중심 원점.
	struct CP { XMFLOAT3 pos; XMFLOAT2 uv; }; // 20B
	const int P = 16;
	static std::vector<CP> cps; cps.clear(); cps.reserve(P * P * 4);
	auto mk = [&](float u, float v) { CP c; c.pos = XMFLOAT3((u - 0.5f) * worldSize, 0.f, (v - 0.5f) * worldSize); c.uv = XMFLOAT2(u, v); return c; };
	for (int pz = 0; pz < P; ++pz) for (int px = 0; px < P; ++px)
	{
		float u0 = (float)px / P, u1 = (float)(px + 1) / P, v0 = (float)pz / P, v1 = (float)(pz + 1) / P;
		cps.push_back(mk(u0, v0)); cps.push_back(mk(u1, v0)); cps.push_back(mk(u1, v1)); cps.push_back(mk(u0, v1)); // BL,BR,TR,TL
	}
	UINT cpBytes = (UINT)(cps.size() * sizeof(CP));
	ensure(_tessCP, _tessCPMapped, _tessCPCap, cpBytes);
	memcpy(_tessCPMapped, cps.data(), cpBytes);

	float tcb[8] = { (float)N, cell, half, _tessFactor, 0, 0, 0, 0 };
	D3D12_VERTEX_BUFFER_VIEW vbv{ _tessCP->GetGPUVirtualAddress(), cpBytes, sizeof(CP) };
	_cmdList->SetPipelineState((_wireframe && _tessWirePSO) ? _tessWirePSO.Get() : _tessPSO.Get());
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, ctx.cb);                                // b0
	_cmdList->SetGraphicsRootShaderResourceView(2, _tessHeights->GetGPUVirtualAddress());   // t1 = 하이트맵
	_cmdList->SetGraphicsRoot32BitConstants(4, 8, tcb, 0);                                  // b1 = 테셀 상수
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
	_cmdList->IASetVertexBuffers(0, 1, &vbv);
	_cmdList->DrawInstanced((UINT)cps.size(), 1, 0, 0);
}

// 씬의 ParticleSystem 입자들을 GPU 인스턴스드 빌보드 쿼드로 렌더 (가산 블렌드). ctx.cb = b0(VP).
void D3D12Device::RenderParticles(const RenderContext& ctx)
{
	using namespace DirectX;
	if (!_gameScene || !_particlePSO) return;

	struct PInst { XMFLOAT3 pos; float size; XMFLOAT3 col; float pad; }; // 32B (셰이더 Particle 와 동일 stride)
	static std::vector<PInst> insts; insts.clear();
	for (auto& kv : _gameScene->GetCreatedObjects())
	{
		auto& o = kv.second; if (!o || !o->IsActive()) continue;
		auto ps = std::dynamic_pointer_cast<ParticleSystem>(o->GetRenderer()); if (!ps) continue;
		float sz0 = ps->Size(), sz1 = ps->SizeEnd(); const Vec3& ce = ps->ColorEnd();
		for (auto& p : ps->Particles())
		{
			float life01 = p.maxLife > 0.f ? p.life / p.maxLife : 1.f; // 1=갓태어남 → 0=소멸
			float t = 1.f - life01;                                     // 수명 진행도
			float sz = sz0 + (sz1 - sz0) * t;                           // 크기 보간
			// 색은 입자 자체색(p.col, 모드별 랜덤) → ColorEnd 로 보간, 알파 페이드(life01)
			float fade = life01;
			XMFLOAT3 c{ (p.col.x + (ce.x - p.col.x) * t) * fade, (p.col.y + (ce.y - p.col.y) * t) * fade, (p.col.z + (ce.z - p.col.z) * t) * fade };
			insts.push_back({ p.pos, sz, c, 0.f });
		}
	}
	// Billboard 컴포넌트도 같은 빌보드 인스턴스로 수집 (1개 = 정적 쿼드)
	for (auto& kv : _gameScene->GetCreatedObjects())
	{
		auto& o = kv.second; if (!o || !o->IsActive()) continue;
		auto bb = std::dynamic_pointer_cast<Billboard>(o->GetRenderer()); if (!bb) continue;
		auto t = o->GetTransform(); Vec3 p = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
		insts.push_back({ p, bb->Size(), bb->Tint(), 0.f });
	}
	if (insts.empty()) return;

	UINT bytes = (UINT)(insts.size() * sizeof(PInst));
	if (_partInstCap < bytes)
	{
		_partInst.Reset();
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = bytes + 8192; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_partInst)), "PartInst buffer");
		_partInstCap = bytes + 8192;
		D3D12_RANGE nr{ 0, 0 }; _partInst->Map(0, &nr, &_partInstMapped);
	}
	memcpy(_partInstMapped, insts.data(), bytes);

	// 카메라 빌보드 기저 (right/up)
	XMVECTOR fwd = _camera.Forward();
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), fwd));
	XMVECTOR up = XMVector3Cross(fwd, right);
	XMFLOAT3 r3, u3; XMStoreFloat3(&r3, right); XMStoreFloat3(&u3, up);
	float bill[8] = { r3.x, r3.y, r3.z, 0.f, u3.x, u3.y, u3.z, 0.f };

	_cmdList->SetPipelineState(_particlePSO.Get());
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, ctx.cb);                              // b0 = VP
	_cmdList->SetGraphicsRootShaderResourceView(2, _partInst->GetGPUVirtualAddress());   // t1 = 파티클 StructuredBuffer
	_cmdList->SetGraphicsRoot32BitConstants(4, 8, bill, 0);                              // b1 = 빌보드 기저
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->DrawInstanced(6, (UINT)insts.size(), 0, 0);                                // VB 없음(절차적)
}
