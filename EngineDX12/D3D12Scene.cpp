#include "ModelScene.h"
#include "D3D12Device.h"   // GPU 인프라 백포인터(_dev) — friend 접근
#include "MeshLoader.h"
#include "TextureLoader.h"
#include <filesystem>
#include <ppl.h>   // 병렬 스키닝

using namespace DirectX;

// 모델 폴더에서 적당한 .clip 선택 (<stem>.clip → Idle/Sprint → 첫 .clip)
static std::wstring FindClip(const std::wstring& dir, const std::wstring& stem)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	for (const std::wstring& cand : { stem + L".clip", std::wstring(L"Idle.clip"), std::wstring(L"Sprint.clip") })
		if (fs::exists(dir + cand, ec)) return dir + cand;
	for (auto& e : fs::directory_iterator(dir, ec))
		if (e.path().extension() == L".clip") return e.path().wstring();
	return L"";
}

// 모델 폴더의 .clip 목록 스캔 (클립 전환 UI 용)
void ModelScene::Load(const std::wstring& meshPath)
{
	if (_dev->_device) _dev->WaitForGpu();

	namespace fs = std::filesystem;
	fs::path mp(meshPath);
	_modelDir = mp.parent_path().wstring() + L"\\";  // 메인 모델 스폰 경로용(_modelStem)
	_modelStem = mp.stem().wstring();

	_cpuVerts.clear(); _cpuIndices.clear();
	CreateCubeGeometry();      // 바닥 지오메트리 + VB/IB (모델은 ModelAnimator GameObject 로 분리)
	CreateASBuffers();         // 바닥 BLAS + TLAS 인프라
	_dev->Log("Floor scene built (" + std::to_string(_vertexCount) + " verts)");
}

// ───────────────────────────────────────────────────────────
// 씬 리소스 — 모델/바닥 지오메트리, CPU 스키닝, 디퓨즈 텍스처
// ───────────────────────────────────────────────────────────
void ModelScene::CreateCubeGeometry()
{
	std::vector<uint32_t> indices;

	_modelIndexCount = 0; // 모델 분리됨 — 전부 바닥 인덱스
	_modelMin = XMFLOAT3(-2.f, 0.f, -2.f); _modelMax = XMFLOAT3(2.f, 3.f, 2.f); // 카메라 포커스 기본 박스

	// ── 바닥 (평면 또는 V1 절차적 터레인 하이트맵) — 모델 정점 뒤에 추가 ──
	{
		const float g = _groundSize;
		const XMFLOAT3 fc(0.90f, 0.12f, 0.10f), t(1, 0, 0);
		if (_terrain)
		{
			const int N = 56; float step = 2.f * g / N;
			auto hgt = [](float x, float z) { return sinf(x * 0.6f) * cosf(z * 0.55f) * 0.5f + sinf(x * 1.7f + z * 0.9f) * 0.16f; };
			uint32 base = (uint32)_cpuVerts.size();
			for (int zi = 0; zi <= N; ++zi) for (int xi = 0; xi <= N; ++xi)
			{
				float x = -g + xi * step, z = -g + zi * step, y = hgt(x, z);
				float hx = hgt(x + 0.1f, z) - hgt(x - 0.1f, z), hz = hgt(x, z + 0.1f) - hgt(x, z - 0.1f);
				XMFLOAT3 nrm; XMStoreFloat3(&nrm, XMVector3Normalize(XMVectorSet(-hx / 0.2f, 1.f, -hz / 0.2f, 0.f)));
				_cpuVerts.push_back({ XMFLOAT3(x, y, z), nrm, fc, XMFLOAT2(xi / (float)N, zi / (float)N), t });
			}
			for (int zi = 0; zi < N; ++zi) for (int xi = 0; xi < N; ++xi)
			{
				uint32 i0 = base + zi * (N + 1) + xi, i1 = i0 + 1, i2 = i0 + (N + 1), i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
				indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
			}
		}
		else
		{
			const XMFLOAT3 n(0, 1, 0); const XMFLOAT2 z(0, 0);
			uint32 b = (uint32)_cpuVerts.size();
			_cpuVerts.push_back({ XMFLOAT3(-g,0, g), n, fc, z, t }); _cpuVerts.push_back({ XMFLOAT3(g,0, g), n, fc, z, t });
			_cpuVerts.push_back({ XMFLOAT3( g,0,-g), n, fc, z, t }); _cpuVerts.push_back({ XMFLOAT3(-g,0,-g), n, fc, z, t });
			indices.push_back(b); indices.push_back(b + 1); indices.push_back(b + 2);
			indices.push_back(b); indices.push_back(b + 2); indices.push_back(b + 3);
		}
	}

	_vertexCount = (UINT)_cpuVerts.size();
	_indexCount = (UINT)indices.size();
	_cpuIndices = indices; // RT 집계 페치용 CPU 미러 (모델+바닥, 정적)
	const size_t vbSize = _cpuVerts.size() * sizeof(Vtx);
	const size_t ibSize = indices.size() * sizeof(uint32_t);

	// VB = 업로드힙 + 영속 매핑 (스키닝으로 매 프레임 갱신). 초기엔 바인드 포즈.
	_vb = _dev->CreateUploadBuffer(nullptr, vbSize);
	{
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(_vb->Map(0, &noRead, &_vbMapped), "Map VB");
		memcpy(_vbMapped, _cpuVerts.data(), vbSize);
	}
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbv.StrideInBytes = sizeof(Vtx);
	_vbv.SizeInBytes = (UINT)vbSize;

	_ib = _dev->CreateUploadBuffer(indices.data(), ibSize);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibv.Format = DXGI_FORMAT_R32_UINT;
	_ibv.SizeInBytes = (UINT)ibSize;
}

// 매 프레임 CPU 갱신 — (스키닝 or 바인드) 베이스 월드 → 기즈모 _modelMatrix 적용 → VB 갱신.
// _modelMatrix 를 정점에 직접 적용하므로 BLAS(매프레임 재빌드)·래스터·DDGI 가 모두 일치 (RT 그림자 따라옴).

// ───────────────────────────────────────────────────────────
// 레이트레이싱 가속구조 (BLAS/TLAS) — 모델+바닥 합본 VB/IB 가 BLAS 소스
// ───────────────────────────────────────────────────────────
static void FillBlasGeom(ID3D12Resource* vb, ID3D12Resource* ib, UINT vcount, UINT icount,
                         D3D12_RAYTRACING_GEOMETRY_DESC& geom)
{
	geom = {};
	geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geom.Triangles.VertexBuffer.StartAddress = vb->GetGPUVirtualAddress();
	geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vtx);
	geom.Triangles.VertexCount = vcount;
	geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geom.Triangles.IndexBuffer = ib->GetGPUVirtualAddress();
	geom.Triangles.IndexCount = icount;
	geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
}

void ModelScene::CreateASBuffers()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geom{};
	FillBlasGeom(_vb.Get(), _ib.Get(), _vertexCount, _indexCount, geom);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasIn{};
	blasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	blasIn.NumDescs = 1;
	blasIn.pGeometryDescs = &geom;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo{};
	_dev->_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasIn, &blasInfo);
	_blas        = _dev->CreateDefaultBuffer(blasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_blasScratch = _dev->CreateDefaultBuffer(blasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// 인스턴스 버퍼 — MAX_INSTANCES 분 (영속 매핑, 매 프레임 갱신). 슬롯0=모델 단위행렬.
	_instanceBuffer = _dev->CreateUploadBuffer(nullptr, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * MAX_INSTANCES);
	{
		D3D12_RANGE nr{ 0, 0 }; ThrowIfFailed(_instanceBuffer->Map(0, &nr, &_instanceMapped), "Map instances");
		D3D12_RAYTRACING_INSTANCE_DESC inst{};
		inst.Transform[0][0] = 1.f; inst.Transform[1][1] = 1.f; inst.Transform[2][2] = 1.f;
		inst.InstanceMask = 0xFF; inst.AccelerationStructure = _blas->GetGPUVirtualAddress();
		memcpy(_instanceMapped, &inst, sizeof(inst)); _instanceCount = 1;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasIn{};
	tlasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	tlasIn.NumDescs = MAX_INSTANCES; // 최대치로 사이징 (빌드 시 실제 count 사용)
	tlasIn.InstanceDescs = _instanceBuffer->GetGPUVirtualAddress();
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo{};
	_dev->_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasIn, &tlasInfo);
	_tlas        = _dev->CreateDefaultBuffer(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, true);
	_tlasScratch = _dev->CreateDefaultBuffer(tlasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// 초기 빌드 (1회) — 정적이면 이것만으로 충분, 스키닝이면 매 프레임 RecordBuildAS 재빌드
	ThrowIfFailed(_dev->_allocators[_dev->_frameIndex]->Reset(), "AS alloc reset");
	ThrowIfFailed(_dev->_cmdList->Reset(_dev->_allocators[_dev->_frameIndex].Get(), nullptr), "AS cmd reset");
	RecordBuildAS(_dev->_cmdList.Get());
	ThrowIfFailed(_dev->_cmdList->Close(), "AS cmd close");
	ID3D12CommandList* lists[] = { _dev->_cmdList.Get() };
	_dev->_queue->ExecuteCommandLists(1, lists);
	_dev->WaitForGpu();
}

// 모델+바닥 BLAS 만 빌드 (+ UAV 배리어). TLAS 는 BuildTLAS 가 통합해서.
void ModelScene::RecordBuildModelBLAS(ID3D12GraphicsCommandList4* cmd)
{
	D3D12_RAYTRACING_GEOMETRY_DESC geom{};
	FillBlasGeom(_vb.Get(), _ib.Get(), _vertexCount, _indexCount, geom);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasIn{};
	blasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	blasIn.NumDescs = 1;
	blasIn.pGeometryDescs = &geom;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuild{};
	blasBuild.Inputs = blasIn;
	blasBuild.DestAccelerationStructureData = _blas->GetGPUVirtualAddress();
	blasBuild.ScratchAccelerationStructureData = _blasScratch->GetGPUVirtualAddress();
	cmd->BuildRaytracingAccelerationStructure(&blasBuild, 0, nullptr);
}

// 통합 TLAS — instances(슬롯0=모델 포함) 를 인스턴스 버퍼에 기록 후 빌드.
void ModelScene::BuildTLAS(ID3D12GraphicsCommandList4* cmd, const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances)
{
	_instanceCount = (UINT)(instances.size() < MAX_INSTANCES ? instances.size() : MAX_INSTANCES);
	if (_instanceCount == 0) _instanceCount = 1; // 안전 (모델만)
	memcpy(_instanceMapped, instances.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * _instanceCount);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasIn{};
	tlasIn.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasIn.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasIn.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	tlasIn.NumDescs = _instanceCount;
	tlasIn.InstanceDescs = _instanceBuffer->GetGPUVirtualAddress();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
	tlasBuild.Inputs = tlasIn;
	tlasBuild.DestAccelerationStructureData = _tlas->GetGPUVirtualAddress();
	tlasBuild.ScratchAccelerationStructureData = _tlasScratch->GetGPUVirtualAddress();
	cmd->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);

	D3D12_RESOURCE_BARRIER ub{};
	ub.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; ub.UAV.pResource = _tlas.Get();
	cmd->ResourceBarrier(1, &ub);
}

// 모델 단독(초기 빌드/폴백) — 모델 BLAS + 단일 인스턴스 TLAS
void ModelScene::RecordBuildAS(ID3D12GraphicsCommandList4* cmd)
{
	RecordBuildModelBLAS(cmd);
	D3D12_RESOURCE_BARRIER ub{};
	ub.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; ub.UAV.pResource = _blas.Get();
	cmd->ResourceBarrier(1, &ub);

	D3D12_RAYTRACING_INSTANCE_DESC inst{};
	inst.Transform[0][0] = 1.f; inst.Transform[1][1] = 1.f; inst.Transform[2][2] = 1.f;
	inst.InstanceMask = 0xFF; inst.AccelerationStructure = _blas->GetGPUVirtualAddress();
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> one{ inst };
	BuildTLAS(cmd, one);
}
