#include "ModelAnimator.h"
#include "D3D12Device.h"
#include "GameObject.h"
#include "Transform.h"
#include "Define.h"
#include "TimeManager.h"
#include "TextureLoader.h"
#include "RtBlas.h"
#include "ResourceManager.h"
#include "EditorUtil.h"   // MaterialSlotGUI (공용 머티리얼 슬롯)
#include "imgui.h"
#include <filesystem>
#include <unordered_map>
#include <ppl.h>   // 병렬 스키닝 (concurrency::parallel_for)

using namespace DirectX;
namespace fs = std::filesystem;

static ComPtr<ID3D12Resource> MakeUploadA(ID3D12Device* dev, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "ModelAnimator buffer");
	return buf;
}

static std::wstring FindClipA(const std::wstring& dir, const std::wstring& stem)
{
	std::error_code ec;
	for (const std::wstring& cand : { stem + L".clip", std::wstring(L"Idle.clip"), std::wstring(L"Sprint.clip") })
		if (fs::exists(dir + cand, ec)) return dir + cand;
	for (auto& e : fs::directory_iterator(dir, ec))
		if (e.path().extension() == L".clip") return e.path().wstring();
	return L"";
}

void ModelAnimator::ScanClips()
{
	_clips.clear(); std::error_code ec;
	for (auto& e : fs::directory_iterator(_modelDir, ec))
		if (e.path().extension() == L".clip") _clips.push_back(e.path().wstring());
}

// ── 공유 모델 캐시 (DX11 의 리소스 공유에 대응) ──
// 같은 .mesh 재스폰 시 파일 읽기/탱전트/텍스처 디코드·업로드·GPU 대기 없이 GPU 리소스 공유.
// CPU 메시 데이터는 복사(저렴), 비싼 GPU 리소스(IB/텍스처/SRV힙)는 ComPtr 공유 → 스폰 끊김 제거.
struct CachedModel
{
	std::vector<LoadedBone> bones; std::vector<SkinVtx> skinSrc; std::vector<uint32> indices;
	std::vector<SubMesh> submeshes;
	float modelScale = 1.f; DirectX::XMFLOAT3 modelOffset{};
	std::vector<std::wstring> clips; std::wstring modelDir, modelStem;
	ComPtr<ID3D12Resource> ib; D3D12_INDEX_BUFFER_VIEW ibv{};
	std::vector<ComPtr<ID3D12Resource>> matResources; ComPtr<ID3D12Resource> whiteTex;
	ComPtr<ID3D12DescriptorHeap> srvHeap; UINT srvInc = 0; uint32 matCount = 0;
	std::vector<uint32> subMatSlot; bool hasTexture = false;
};
static std::unordered_map<std::wstring, std::shared_ptr<CachedModel>> s_modelCache;

bool ModelAnimator::Load(const std::wstring& meshPath)
{
	if (!_dev) return false;
	_clip = AnimClip{}; _animated = false; _curClip = 0; _animTime = 0.f;

	auto createWorldVB = [&]()
	{
		const size_t vbSize = (size_t)_vtxCount * sizeof(Vtx);
		_vb = MakeUploadA(_dev->_device.Get(), vbSize);
		D3D12_RANGE nr{ 0, 0 }; _vb->Map(0, &nr, &_vbMapped);
		_vbv.BufferLocation = _vb->GetGPUVirtualAddress(); _vbv.StrideInBytes = sizeof(Vtx); _vbv.SizeInBytes = (UINT)vbSize;
		_world.assign(_vtxCount, Vtx{});
	};
	auto finishClip = [&]()
	{
		std::wstring clipPath = FindClipA(_modelDir, _modelStem);
		if (!clipPath.empty()) _animated = LoadClip(clipPath, _clip);
		Skin(0); _skinnedOnce = true; _lastFrame = 0;
	};

	// ── 캐시 히트: 파일/텍스처 재로드 없이 공유 (스폰 즉시) ──
	auto it = s_modelCache.find(meshPath);
	if (it != s_modelCache.end())
	{
		auto& c = *it->second;
		_bonesData = c.bones; _skinSrc = c.skinSrc; _indices = c.indices; _submeshes = c.submeshes;
		_modelScale = c.modelScale; _modelOffset = c.modelOffset; _clips = c.clips;
		_modelDir = c.modelDir; _modelStem = c.modelStem;
		_ib = c.ib; _ibv = c.ibv;                 // GPU 리소스 공유
		_matResources = c.matResources; _whiteTex = c.whiteTex; _srvHeap = c.srvHeap;
		_srvInc = c.srvInc; _matCount = c.matCount; _subMatSlot = c.subMatSlot; _hasTexture = c.hasTexture;
		_vtxCount = (uint32)_skinSrc.size(); _idxCount = (uint32)_indices.size();
		createWorldVB();
		finishClip();
		return true;
	}

	// ── 캐시 미스: 최초 로드 ──
	fs::path mp(meshPath);
	_modelDir = mp.parent_path().wstring() + L"\\";
	_modelStem = mp.stem().wstring();
	_bonesData.clear(); _skinSrc.clear(); _indices.clear(); _submeshes.clear();

	if (!LoadMeshSkinned(meshPath, _bonesData, _skinSrc, _indices, &_submeshes)) return false;
	GenerateTangents(_skinSrc, _indices);

	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
	for (auto& v : _skinSrc)
	{
		mn.x = min(mn.x, v.pos.x); mn.y = min(mn.y, v.pos.y); mn.z = min(mn.z, v.pos.z);
		mx.x = max(mx.x, v.pos.x); mx.y = max(mx.y, v.pos.y); mx.z = max(mx.z, v.pos.z);
	}
	_modelScale = 2.2f / max(mx.y - mn.y, 0.001f);
	_modelOffset = XMFLOAT3((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f);
	_vtxCount = (uint32)_skinSrc.size();
	_idxCount = (uint32)_indices.size();

	ScanClips();
	CreateMaterials();

	// 공유 IB (인덱스는 인스턴스 불변)
	const size_t ibSize = (size_t)_idxCount * sizeof(uint32);
	_ib = MakeUploadA(_dev->_device.Get(), ibSize);
	void* p = nullptr; D3D12_RANGE nr2{ 0, 0 }; _ib->Map(0, &nr2, &p); memcpy(p, _indices.data(), ibSize); _ib->Unmap(0, nullptr);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress(); _ibv.Format = DXGI_FORMAT_R32_UINT; _ibv.SizeInBytes = (UINT)ibSize;

	createWorldVB();
	finishClip();

	// 캐시에 등록
	auto c = std::make_shared<CachedModel>();
	c->bones = _bonesData; c->skinSrc = _skinSrc; c->indices = _indices;
	c->submeshes = _submeshes; c->modelScale = _modelScale; c->modelOffset = _modelOffset;
	c->clips = _clips; c->modelDir = _modelDir; c->modelStem = _modelStem;
	c->ib = _ib; c->ibv = _ibv;
	c->matResources = _matResources; c->whiteTex = _whiteTex; c->srvHeap = _srvHeap;
	c->srvInc = _srvInc; c->matCount = _matCount; c->subMatSlot = _subMatSlot; c->hasTexture = _hasTexture;
	s_modelCache[meshPath] = c;
	return true;
}

// .mmat → 서브메시 슬롯별 디퓨즈/노멀/스펙 텍스처 + 연속 SRV 힙 (ModelScene 미러)
void ModelAnimator::CreateMaterials()
{
	std::unordered_map<std::string, MatTex> matMap = LoadMaterials(_modelDir + _modelStem + L".mmat", _modelDir);
	std::unordered_map<std::string, uint32> stemSlot; std::vector<std::string> slotStems;
	_subMatSlot.assign(_submeshes.size(), 0);
	for (size_t i = 0; i < _submeshes.size(); ++i)
	{
		const std::string& nm = _submeshes[i].materialName;
		auto it = stemSlot.find(nm);
		if (it == stemSlot.end()) { uint32 s = (uint32)slotStems.size(); stemSlot[nm] = s; slotStems.push_back(nm); _subMatSlot[i] = s; }
		else _subMatSlot[i] = it->second;
	}
	_matCount = (uint32)slotStems.size();
	if (_matCount == 0) { _hasTexture = false; return; }

	// 전용 업로드 커맨드리스트+펜스 (메인 렌더 파이프라인/공유 cmdList 미사용 — 전체 플러시 회피)
	std::vector<ComPtr<ID3D12Resource>> uploads;
	ComPtr<ID3D12CommandAllocator> al; ComPtr<ID3D12GraphicsCommandList> cl;
	_dev->_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
	_dev->_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al.Get(), nullptr, IID_PPV_ARGS(&cl));

	auto makeTex = [&](const std::vector<uint8_t>& px, uint32 tw, uint32 th) -> ComPtr<ID3D12Resource>
	{
		ComPtr<ID3D12Resource> outTex;
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC td{}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		td.Width = tw; td.Height = th; td.DepthOrArraySize = 1; td.MipLevels = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
		ThrowIfFailed(_dev->_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outTex)), "anim Create Tex");
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
		_dev->_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);
		ComPtr<ID3D12Resource> up = _dev->CreateUploadBuffer(nullptr, (size_t)total);
		uint8_t* m = nullptr; D3D12_RANGE nr{ 0, 0 }; up->Map(0, &nr, (void**)&m);
		for (UINT y = 0; y < rows; ++y)
			memcpy(m + fp.Offset + (size_t)y * fp.Footprint.RowPitch, px.data() + (size_t)y * tw * 4, (size_t)tw * 4);
		up->Unmap(0, nullptr); uploads.push_back(up);
		D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = outTex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
		D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = up.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
		cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = outTex.Get();
		b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; cl->ResourceBarrier(1, &b);
		return outTex;
	};
	{ std::vector<uint8_t> white(4, 255); _whiteTex = makeTex(white, 1, 1); }

	// 텍스처 파일 경로 수집 (슬롯×3)
	const uint32 nTex = _matCount * 3;
	std::vector<std::wstring> files(nTex);
	for (uint32 s = 0; s < _matCount; ++s)
	{
		MatTex mt; auto it = matMap.find(slotStems[s]); if (it != matMap.end()) mt = it->second;
		files[s * 3 + 0] = mt.diffuse; files[s * 3 + 1] = mt.normal; files[s * 3 + 2] = mt.spec;
	}
	// WIC 디코드 병렬화 (첫 로드 비용의 핵심 — CPU 디코드를 코어 분산, GPU 생성은 직렬)
	struct Dec { std::vector<uint8_t> px; uint32 w = 0, h = 0; bool ok = false; };
	std::vector<Dec> dec(nTex);
	concurrency::parallel_for(uint32(0), nTex, [&](uint32 i)
	{
		if (files[i].empty()) return;
		CoInitializeEx(nullptr, COINIT_MULTITHREADED); // 워커 스레드 WIC COM
		Dec d; if (LoadImageRGBA(files[i], d.px, d.w, d.h)) d.ok = true;
		CoUninitialize();
		dec[i] = std::move(d);
	});
	// GPU 텍스처 생성 (직렬, cl 기록)
	_matResources.resize(nTex);
	for (uint32 i = 0; i < nTex; ++i)
		_matResources[i] = dec[i].ok ? makeTex(dec[i].px, dec[i].w, dec[i].h) : _whiteTex;
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() }; _dev->_queue->ExecuteCommandLists(1, lists);
	// 전용 펜스 대기 (업로드만 — 렌더 프레임 펜스 미교란)
	ComPtr<ID3D12Fence> fence; _dev->_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_dev->_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); }
	CloseHandle(ev);

	D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = _matCount * 3;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_dev->_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "anim srv heap");
	_srvInc = _dev->_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE h = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	for (uint32 i = 0; i < _matCount * 3; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC sd{}; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sd.Texture2D.MipLevels = 1;
		_dev->_device->CreateShaderResourceView(_matResources[i].Get(), &sd, h); h.ptr += _srvInc;
	}
	_hasTexture = true;
}

void ModelAnimator::SetClipIndex(int i)
{
	if (i < 0 || i >= (int)_clips.size()) return;
	_curClip = i; _animTime = 0.f;
	_animated = LoadClip(_clips[i], _clip);
	_skinnedOnce = false; // 일시정지 상태에서도 새 포즈 반영
}

void ModelAnimator::Skin(uint32 frame)
{
	if (_vtxCount == 0) return;
	const size_t nb = _bonesData.size();
	const bool anim = _animated && _clip.frameCount > 0 && nb > 0;

	std::vector<XMMATRIX> global, skin;
	if (anim)
	{
		global.resize(nb); skin.resize(nb);
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

	// GameObject 월드행렬 = 배치 트랜스폼 (없으면 단위)
	XMMATRIX M = XMMatrixIdentity();
	if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); M = XMLoadFloat4x4(&wm); }
	XMVECTOR off = XMVectorSet(_modelOffset.x, _modelOffset.y, _modelOffset.z, 0.f);
	const XMFLOAT3 modelC(0.82f, 0.78f, 0.72f);
	const float scale = _modelScale; const bool useAnim = anim;
	const SkinVtx* src = _skinSrc.data(); Vtx* dst = _world.data();
	const XMMATRIX* skinM = skin.empty() ? nullptr : skin.data();

	// 정점 변환 병렬화 (코어 분산 — Debug 단일스레드 XMMath 병목 완화)
	concurrency::parallel_for(uint32(0), _vtxCount, [&](uint32 i)
	{
		const SkinVtx& sv = src[i];
		XMVECTOR bp = XMLoadFloat3(&sv.pos), bn = XMLoadFloat3(&sv.nrm), bt = XMLoadFloat3(&sv.tan);
		XMVECTOR p, n, t;
		if (useAnim && skinM)
		{
			p = n = t = XMVectorZero(); float wsum = 0.f;
			for (int j = 0; j < 4; ++j)
			{
				float w = sv.wgt[j]; uint32 bi = sv.idx[j];
				if (w <= 0.f || bi >= nb) continue;
				p = XMVectorAdd(p, XMVectorScale(XMVector3Transform(bp, skinM[bi]), w));
				n = XMVectorAdd(n, XMVectorScale(XMVector3TransformNormal(bn, skinM[bi]), w));
				t = XMVectorAdd(t, XMVectorScale(XMVector3TransformNormal(bt, skinM[bi]), w));
				wsum += w;
			}
			if (wsum < 1e-4f) { p = bp; n = bn; t = bt; }
		}
		else { p = bp; n = bn; t = bt; }

		XMVECTOR wp = XMVectorScale(XMVectorSubtract(p, off), scale);
		wp = XMVector3Transform(wp, M);
		n = XMVector3TransformNormal(n, M);
		t = XMVector3TransformNormal(t, M);
		XMStoreFloat3(&dst[i].pos, wp);
		XMStoreFloat3(&dst[i].nrm, XMVector3Normalize(n));
		XMStoreFloat3(&dst[i].tan, XMVector3Normalize(t));
		dst[i].col = modelC; dst[i].uv = sv.uv;
	});

	// AABB 직렬 리덕션 (가벼움)
	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
	for (uint32 i = 0; i < _vtxCount; ++i)
	{
		const XMFLOAT3& P = dst[i].pos;
		mn.x = min(mn.x, P.x); mn.y = min(mn.y, P.y); mn.z = min(mn.z, P.z);
		mx.x = max(mx.x, P.x); mx.y = max(mx.y, P.y); mx.z = max(mx.z, P.z);
	}
	_aabbMin = mn; _aabbMax = mx;
	memcpy(_vbMapped, _world.data(), (size_t)_vtxCount * sizeof(Vtx));
}

void ModelAnimator::TransformBoundingBox()
{
	if (_vtxCount == 0) { Renderer::TransformBoundingBox(); return; }
	_boundingBox.Center  = XMFLOAT3((_aabbMin.x + _aabbMax.x) * 0.5f, (_aabbMin.y + _aabbMax.y) * 0.5f, (_aabbMin.z + _aabbMax.z) * 0.5f);
	_boundingBox.Extents = XMFLOAT3((_aabbMax.x - _aabbMin.x) * 0.5f, (_aabbMax.y - _aabbMin.y) * 0.5f, (_aabbMax.z - _aabbMin.z) * 0.5f);
}

// RT 통합 — AS 패스에서 호출(Draw 전). 프레임/트랜스폼이 바뀐 경우에만 재스키닝(낭비 제거).
void ModelAnimator::UpdateWorld()
{
	if (!_dev || _vtxCount == 0) return;
	const bool anim = _animated && _clip.frameCount > 0;
	if (anim && _playing) _animTime += DT * _speed;

	uint32 frame = 0;
	if (anim)
	{
		uint32 raw = (uint32)(_animTime * _clip.frameRate);
		frame = _loop ? (raw % _clip.frameCount) : (raw < _clip.frameCount ? raw : _clip.frameCount - 1);
	}
	uint32 ver = 0; if (auto t = GetTransform()) ver = t->Version();

	// 프레임도 트랜스폼도 그대로면 스킵 (60fps 렌더 × 30fps 클립 → 중복 프레임 생략)
	if (_skinnedOnce && frame == _lastFrame && ver == _bakedVer) return;

	Skin(frame);
	_lastFrame = frame; _bakedVer = ver; _skinnedOnce = true; _blasDirty = true;
}

void ModelAnimator::RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || _vtxCount == 0) return;
	if (_blas && !_blasDirty) return; // 변경 없으면 기존 BLAS 유지
	RtBlas::Build(_dev->_device.Get(), cmd, _vb.Get(), _ib.Get(),
	              _vtxCount, _idxCount, sizeof(Vtx), _blas, _blasScratch);
	_blasDirty = false;
}

void ModelAnimator::Draw(const RenderContext& ctx)
{
	if (!_dev || _vtxCount == 0) return;
	// 스키닝/시간전진은 AS 패스의 UpdateWorld 에서 수행됨 (여기선 그리기만)

	D3D12Device& d = *_dev;
	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, ctx.cb); // 카메라별 CB (Scene/Game)
	cmd->SetGraphicsRootShaderResourceView(1, d._scene._tlas->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);

	// per-object 머티리얼 루트상수 — PBR/틴트는 _material 오버라이드(인스펙터 편집), 텍스처는 .mmat 슬롯
	struct { uint32 mode; float met, rough, emis, tr, tg, tb, pad; } mc{
		(_hasTexture && !_submeshes.empty()) ? 1u : 2u,
		_material->_metallic, _material->_roughness, _material->_emissive,
		_material->_diffuse.x, _material->_diffuse.y, _material->_diffuse.z, 0.f };
	cmd->SetGraphicsRoot32BitConstants(4, 8, &mc, 0);

	if (_hasTexture && !_submeshes.empty())
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		D3D12_GPU_DESCRIPTOR_HANDLE base = _srvHeap->GetGPUDescriptorHandleForHeapStart();
		for (size_t i = 0; i < _submeshes.size(); ++i)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE hh = base;
			hh.ptr += SIZE_T(_subMatSlot[i]) * 3 * _srvInc; // 슬롯×3 디스크립터
			cmd->SetGraphicsRootDescriptorTable(3, hh);
			cmd->DrawIndexedInstanced(_submeshes[i].indexCount, 1, _submeshes[i].indexStart, 0, 0);
		}
	}
	else
		cmd->DrawIndexedInstanced(_idxCount, 1, 0, 0, 0);
}

// 선택 아웃라인 — 현재 스킨 포즈의 월드 VB/IB 를 그대로 드로우 (PSO/CB 는 호출측)
void ModelAnimator::RecordOutline(ID3D12GraphicsCommandList4* cmd)
{
	if (!_vb || _idxCount == 0) return;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced(_idxCount, 1, 0, 0, 0);
}

void ModelAnimator::OnInspectorGUI()
{
	ImGui::SeparatorText("ModelAnimator");
	ImGui::Text("Mesh: %ls  (%u verts, %u bones)", _modelStem.c_str(), _vtxCount, (uint32)_bonesData.size());
	if (!_clips.empty())
	{
		std::string names; // \0 구분 콤보 항목
		for (auto& c : _clips) { names += std::string(fs::path(c).stem().string()); names.push_back('\0'); }
		names.push_back('\0');
		int idx = _curClip;
		if (ImGui::Combo("Clip", &idx, names.c_str())) SetClipIndex(idx);
		ImGui::Text("Frames %u  @ %.0f fps", _clip.frameCount, _clip.frameRate);
	}
	else ImGui::TextDisabled("(no clips)");
	ImGui::Checkbox("Playing", &_playing); ImGui::SameLine();
	ImGui::Checkbox("Loop", &_loop);
	ImGui::DragFloat("Speed", &_speed, 0.02f, 0.f, 4.f);

	// ── 머티리얼 (모델 전체 오버라이드 — 틴트는 .mmat 텍스처에 곱해짐) ──
	ImGui::SeparatorText("Material");
	ImGui::ColorEdit3("Tint", &_material->_diffuse.x);
	ImGui::DragFloat("Metallic", &_material->_metallic, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Roughness", &_material->_roughness, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Emissive", &_material->_emissive, 0.02f, 0.f, 16.f);
	auto preset = [&](const char* n, Vec3 d, float m, float r) { if (ImGui::SmallButton(n)) { _material->_diffuse = d; _material->_metallic = m; _material->_roughness = r; } ImGui::SameLine(); };
	preset("Default", { 1,1,1 }, 0.f, 0.5f);
	preset("Metal", { 0.9f,0.9f,0.9f }, 1.f, 0.25f);
	preset("Gold", { 1.0f,0.78f,0.34f }, 1.f, 0.2f);
	if (ImGui::SmallButton("Plastic")) { _material->_diffuse = { 1,1,1 }; _material->_metallic = 0.f; _material->_roughness = 0.4f; }
	// 공유 .mat 슬롯 (PBR/틴트 일괄 — 텍스처 슬롯은 .mmat 유지)
	if (_dev) MaterialSlotGUI(_dev->_assetRoot, _material, [this](shared_ptr<Material> m) { SetMaterialRef(m); });
	ImGui::TextDisabled("(텍스처는 .mmat 슬롯, 여기선 PBR/틴트만)");
}
