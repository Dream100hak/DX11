#include "D3D12Device.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include <filesystem>

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
void D3D12Device::ScanClips()
{
	namespace fs = std::filesystem;
	_clips.clear(); _curClip = 0;
	std::error_code ec;
	for (auto& e : fs::directory_iterator(_modelDir, ec))
		if (e.path().extension() == L".clip") _clips.push_back(e.path().wstring());
}

// 런타임 모델 교체 — 상태 리셋 후 메시/텍스처/AS 재구성 (전체 플러시로 GPU 유휴 가정)
void D3D12Device::LoadModelFromFile(const std::wstring& meshPath)
{
	if (_device) WaitForGpu();

	namespace fs = std::filesystem;
	fs::path mp(meshPath);
	_modelDir = mp.parent_path().wstring() + L"\\";
	_modelStem = mp.stem().wstring();
	_modelLabel = _modelStem;

	// 모델 상태 리셋
	_skinSrc.clear(); _submeshes.clear(); _cpuVerts.clear(); _bonesData.clear();
	_subMatSlot.clear(); _matResources.clear();
	_clip = AnimClip{}; _animated = false; _matCount = 0; _hasTexture = false;
	XMStoreFloat4x4(&_modelMatrix, XMMatrixIdentity());
	_modelMatrixInit = true;

	CreateCubeGeometry();      // 메시(_modelDir/_modelStem) + 바닥 + VB/IB
	ScanClips();               // 클립 목록(전환 UI)
	CreateTextureResources();  // .mmat/.mat → 머티리얼 텍스처
	CreateASBuffers();         // BLAS/TLAS (정점/인덱스 수에 맞춰 재생성)
}

// ───────────────────────────────────────────────────────────
// 씬 리소스 — 모델/바닥 지오메트리, CPU 스키닝, 디퓨즈 텍스처
// (D3D12Device.cpp 에서 분리)
// ───────────────────────────────────────────────────────────

void D3D12Device::CreateCubeGeometry()
{
	std::vector<uint32_t> indices;
	const XMFLOAT3 modelC(0.82f, 0.78f, 0.72f);

	// ── 스키닝 .mesh 모델 로드 (본+블렌드+서브메시) + .clip 애니메이션 ──
	{
		// 기본값(직접 호출 시) — Archer
		if (_modelStem.empty())
		{
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			std::wstring dir(exe);
			dir = dir.substr(0, dir.find_last_of(L"\\/"));
			_modelDir = dir + L"\\..\\Resources\\Assets\\Models\\Archer\\";
			_modelStem = L"Archer";
		}
		std::wstring meshPath = _modelDir + _modelStem + L".mesh";

		if (LoadMeshSkinned(meshPath, _bonesData, _skinSrc, indices, &_submeshes))
		{
			// 탄젠트가 0(구 변환)이면 UV로 재생성 → 노멀맵 실효
			GenerateTangents(_skinSrc, indices);

			// 바인드 AABB → 높이 2.2 정규화, x/z 중앙, 바닥 안착 (스키닝 후에도 동일 적용)
			XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
			for (auto& v : _skinSrc)
			{
				mn.x = min(mn.x, v.pos.x); mn.y = min(mn.y, v.pos.y); mn.z = min(mn.z, v.pos.z);
				mx.x = max(mx.x, v.pos.x); mx.y = max(mx.y, v.pos.y); mx.z = max(mx.z, v.pos.z);
			}
			_modelScale = 2.2f / max(mx.y - mn.y, 0.001f);
			_modelOffset = XMFLOAT3((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f);
			_modelVtxCount = (uint32)_skinSrc.size();

			for (auto& v : _skinSrc)
				_cpuVerts.push_back({ XMFLOAT3((v.pos.x - _modelOffset.x) * _modelScale,
				                                (v.pos.y - _modelOffset.y) * _modelScale,
				                                (v.pos.z - _modelOffset.z) * _modelScale), v.nrm, modelC, v.uv, v.tan });

			std::wstring clipPath = FindClip(_modelDir, _modelStem);
			if (!clipPath.empty()) _animated = LoadClip(clipPath, _clip);
		}
	}

	_modelIndexCount = (uint32)indices.size(); // 바닥 추가 전 = 모델 인덱스 수

	// ── 바닥 평면 (빨강) — 모델 정점 뒤에 추가 ──
	{
		const float g = 6.f;
		const XMFLOAT3 fc(0.90f, 0.12f, 0.10f), n(0, 1, 0), t(1, 0, 0);
		const XMFLOAT2 z(0, 0);
		uint32 b = (uint32)_cpuVerts.size();
		_cpuVerts.push_back({ XMFLOAT3(-g,0, g), n, fc, z, t }); _cpuVerts.push_back({ XMFLOAT3(g,0, g), n, fc, z, t });
		_cpuVerts.push_back({ XMFLOAT3( g,0,-g), n, fc, z, t }); _cpuVerts.push_back({ XMFLOAT3(-g,0,-g), n, fc, z, t });
		indices.push_back(b); indices.push_back(b + 1); indices.push_back(b + 2);
		indices.push_back(b); indices.push_back(b + 2); indices.push_back(b + 3);
	}

	_vertexCount = (UINT)_cpuVerts.size();
	_indexCount = (UINT)indices.size();
	const size_t vbSize = _cpuVerts.size() * sizeof(Vtx);
	const size_t ibSize = indices.size() * sizeof(uint32_t);

	// VB = 업로드힙 + 영속 매핑 (스키닝으로 매 프레임 갱신). 초기엔 바인드 포즈.
	_vb = CreateUploadBuffer(nullptr, vbSize);
	{
		D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(_vb->Map(0, &noRead, &_vbMapped), "Map VB");
		memcpy(_vbMapped, _cpuVerts.data(), vbSize);
	}
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbv.StrideInBytes = sizeof(Vtx);
	_vbv.SizeInBytes = (UINT)vbSize;

	_ib = CreateUploadBuffer(indices.data(), ibSize);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibv.Format = DXGI_FORMAT_R32_UINT;
	_ibv.SizeInBytes = (UINT)ibSize;
}

// 매 프레임 CPU 갱신 — (스키닝 or 바인드) 베이스 월드 → 기즈모 _modelMatrix 적용 → VB 갱신.
// _modelMatrix 를 정점에 직접 적용하므로 BLAS(매프레임 재빌드)·래스터·DDGI 가 모두 일치 (RT 그림자 따라옴).
void D3D12Device::UpdateAnimation()
{
	if (_modelVtxCount == 0) return;

	size_t nb = _bonesData.size();
	bool anim = _animated && _clip.frameCount > 0 && nb > 0;

	std::vector<XMMATRIX> skin;
	if (anim)
	{
		uint32 frame = (uint32)(_animTimeAcc * _clip.frameRate) % _clip.frameCount;
		std::vector<XMMATRIX> global(nb); skin.resize(nb);
		for (size_t b = 0; b < nb; ++b)
		{
			const LoadedBone& bone = _bonesData[b];
			XMMATRIX matAnim = XMMatrixIdentity();
			auto it = _clip.bones.find(bone.name);
			if (it != _clip.bones.end() && frame < it->second.size())
			{
				const ClipFrameT& k = it->second[frame];
				matAnim = XMMatrixScaling(k.s.x, k.s.y, k.s.z) *
				          XMMatrixRotationQuaternion(XMLoadFloat4(&k.r)) *
				          XMMatrixTranslation(k.t.x, k.t.y, k.t.z);
			}
			XMMATRIX parent = (bone.parent >= 0 && bone.parent < (int32_t)nb) ? global[bone.parent] : XMMatrixIdentity();
			global[b] = matAnim * parent;
			skin[b] = XMMatrixInverse(nullptr, XMLoadFloat4x4(&bone.bind)) * global[b];
		}
	}

	XMVECTOR off = XMVectorSet(_modelOffset.x, _modelOffset.y, _modelOffset.z, 0.f);
	XMMATRIX M = XMLoadFloat4x4(&_modelMatrix); // 기즈모 트랜스폼
	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f); // 모델 월드 AABB (픽킹용)

	for (uint32 i = 0; i < _modelVtxCount; ++i)
	{
		const SkinVtx& sv = _skinSrc[i];
		XMVECTOR bp = XMLoadFloat3(&sv.pos), bn = XMLoadFloat3(&sv.nrm), bt = XMLoadFloat3(&sv.tan);
		XMVECTOR p, n, t;
		if (anim)
		{
			p = XMVectorZero(); n = XMVectorZero(); t = XMVectorZero();
			float wsum = 0.f;
			for (int j = 0; j < 4; ++j)
			{
				float w = sv.wgt[j]; uint32 bi = sv.idx[j];
				if (w <= 0.f || bi >= nb) continue;
				p = XMVectorAdd(p, XMVectorScale(XMVector3Transform(bp, skin[bi]), w));
				n = XMVectorAdd(n, XMVectorScale(XMVector3TransformNormal(bn, skin[bi]), w));
				t = XMVectorAdd(t, XMVectorScale(XMVector3TransformNormal(bt, skin[bi]), w));
				wsum += w;
			}
			if (wsum < 1e-4f) { p = bp; n = bn; t = bt; }
		}
		else { p = bp; n = bn; t = bt; } // 정적: 바인드 포즈

		// 베이스 월드(정규화) → 기즈모 M 적용
		XMVECTOR wp = XMVectorScale(XMVectorSubtract(p, off), _modelScale);
		wp = XMVector3Transform(wp, M);
		n = XMVector3TransformNormal(n, M);
		t = XMVector3TransformNormal(t, M);
		XMStoreFloat3(&_cpuVerts[i].pos, wp);
		XMStoreFloat3(&_cpuVerts[i].nrm, XMVector3Normalize(n));
		XMStoreFloat3(&_cpuVerts[i].tan, XMVector3Normalize(t));
		XMFLOAT3 P = _cpuVerts[i].pos;
		mn.x = min(mn.x, P.x); mn.y = min(mn.y, P.y); mn.z = min(mn.z, P.z);
		mx.x = max(mx.x, P.x); mx.y = max(mx.y, P.y); mx.z = max(mx.z, P.z);
	}
	_modelMin = mn; _modelMax = mx;
	memcpy(_vbMapped, _cpuVerts.data(), _cpuVerts.size() * sizeof(Vtx));
}

void D3D12Device::CreateTextureResources()
{
	std::wstring base = _modelDir;

	// ── 머티리얼 슬롯 배정: 서브메시 머티리얼명 → 고유 슬롯 ──
	std::unordered_map<std::string, MatTex> matMap = LoadMaterials(base + _modelStem + L".mmat", base);
	std::unordered_map<std::string, uint32> stemSlot;
	std::vector<std::string> slotStems;
	_subMatSlot.resize(_submeshes.size());
	for (size_t i = 0; i < _submeshes.size(); ++i)
	{
		const std::string& nm = _submeshes[i].materialName;
		auto it = stemSlot.find(nm);
		if (it == stemSlot.end()) { uint32 s = (uint32)slotStems.size(); stemSlot[nm] = s; slotStems.push_back(nm); _subMatSlot[i] = s; }
		else _subMatSlot[i] = it->second;
	}
	_matCount = (uint32)slotStems.size();
	if (_matCount == 0) { _hasTexture = false; return; }

	std::vector<ComPtr<ID3D12Resource>> uploads; // WaitForGpu 까지 생존 필요

	ThrowIfFailed(_allocators[_frameIndex]->Reset(), "tex alloc reset");
	ThrowIfFailed(_cmdList->Reset(_allocators[_frameIndex].Get(), nullptr), "tex cmd reset");

	// 원시 RGBA8 픽셀 → DX12 텍스처 생성 + 업로드 복사 기록
	auto makeTex = [&](const std::vector<uint8_t>& px, uint32 tw, uint32 th) -> ComPtr<ID3D12Resource>
	{
		ComPtr<ID3D12Resource> outTex;
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC td{};
		td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		td.Width = tw; td.Height = th; td.DepthOrArraySize = 1; td.MipLevels = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
		ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outTex)), "Create Tex");

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT numRows = 0; UINT64 rowSize = 0, uploadSize = 0;
		_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &numRows, &rowSize, &uploadSize);
		ComPtr<ID3D12Resource> up = CreateUploadBuffer(nullptr, (size_t)uploadSize);
		uint8_t* mapped = nullptr; D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(up->Map(0, &noRead, (void**)&mapped), "Map texUpload");
		for (UINT y = 0; y < numRows; ++y)
			memcpy(mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, px.data() + (size_t)y * tw * 4, (size_t)tw * 4);
		up->Unmap(0, nullptr);
		uploads.push_back(up);

		D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = outTex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
		D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = up.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
		_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		D3D12_RESOURCE_BARRIER bar{};
		bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bar.Transition.pResource = outTex.Get();
		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		_cmdList->ResourceBarrier(1, &bar);
		return outTex;
	};

	// 1×1 흰색 폴백 (텍스처 없는 슬롯/채널용)
	{
		std::vector<uint8_t> white(4, 255);
		_whiteTex = makeTex(white, 1, 1);
	}

	// 파일 로드 → 텍스처(실패 시 흰색)
	auto loadOr = [&](const std::wstring& file) -> ComPtr<ID3D12Resource>
	{
		std::vector<uint8_t> px; uint32 tw = 0, th = 0;
		if (!file.empty() && LoadImageRGBA(file, px, tw, th)) return makeTex(px, tw, th);
		return _whiteTex;
	};

	// 슬롯별 디퓨즈/노멀/스펙 (총 _matCount×3 리소스, 슬롯 순서)
	_matResources.resize(_matCount * 3);
	for (uint32 s = 0; s < _matCount; ++s)
	{
		MatTex mt; auto it = matMap.find(slotStems[s]); if (it != matMap.end()) mt = it->second;
		_matResources[s * 3 + 0] = loadOr(mt.diffuse);
		_matResources[s * 3 + 1] = loadOr(mt.normal);
		_matResources[s * 3 + 2] = loadOr(mt.spec);
	}

	ThrowIfFailed(_cmdList->Close(), "tex cmd close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();

	// 연속 SRV 힙 (_matCount×3, 슬롯당 디퓨즈 t2 / 노멀 t3 / 스펙 t4)
	D3D12_DESCRIPTOR_HEAP_DESC hd{};
	hd.NumDescriptors = _matCount * 3;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "srv heap");

	_srvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE h = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	for (uint32 i = 0; i < _matCount * 3; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sd.Texture2D.MipLevels = 1;
		_device->CreateShaderResourceView(_matResources[i].Get(), &sd, h);
		h.ptr += _srvInc;
	}
	_hasTexture = true;
}
