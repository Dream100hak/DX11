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
	ComPtr<ID3D12Resource> srcVB;                 // 소스 정점 (GPU 스키닝 입력 — 동일 메시 공유)
	DirectX::XMFLOAT3 localMin{}, localMax{};     // 바인드포즈 로컬 AABB
	std::vector<ComPtr<ID3D12Resource>> matResources; ComPtr<ID3D12Resource> whiteTex;
	ComPtr<ID3D12DescriptorHeap> srvHeap; UINT srvInc = 0; uint32 matCount = 0;
	std::vector<uint32> subMatSlot; bool hasTexture = false;
};
static std::unordered_map<std::wstring, std::shared_ptr<CachedModel>> s_modelCache;

bool ModelAnimator::Load(const std::wstring& meshPath)
{
	if (!_dev) return false;
	_curClip = 0; _animTime = 0.f; _prevClip = -1; _fadeDur = 0.f; _prevNorm = 0.f;
	_clipData.clear();

	auto createGpuBufs = [&]()
	{
		// 출력 월드 VB — DEFAULT+UAV (컴퓨트 스키닝이 기록, 래스터/BLAS/집계가 읽음)
		const size_t vbSize = (size_t)_vtxCount * sizeof(Vtx);
		_vb = _dev->CreateDefaultBuffer(vbSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		_vbState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		_vbv.BufferLocation = _vb->GetGPUVirtualAddress(); _vbv.StrideInBytes = sizeof(Vtx); _vbv.SizeInBytes = (UINT)vbSize;
		// 직전 프레임 월드 VB (속도 G버퍼) — UAV 불필요(복사 대상)
		_vbPrev = _dev->CreateDefaultBuffer(vbSize, D3D12_RESOURCE_STATE_COMMON, false);
		_vbPrevState = D3D12_RESOURCE_STATE_COMMON; _vbPrevInit = false;
		// 본 행렬(업로드, nb×64) + SkinParams CB(업로드, 256정렬) — 매프레임 갱신
		const uint32 nb = (uint32)(_bonesData.empty() ? 1 : _bonesData.size());
		_boneBuf = MakeUploadA(_dev->_device.Get(), (size_t)nb * 64); _boneCap = nb;
		D3D12_RANGE nr{ 0, 0 }; _boneBuf->Map(0, &nr, &_boneMapped);
		_skinCB = MakeUploadA(_dev->_device.Get(), 256); _skinCB->Map(0, &nr, &_skinCBMapped);
	};
	auto finishClip = [&]()
	{
		// 역바인드 캐시 (로드 1회 — BuildSkinMatrices 의 매프레임 XMMatrixInverse 제거)
		_invBind.resize(_bonesData.size());
		for (size_t b = 0; b < _bonesData.size(); ++b)
			_invBind[b] = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_bonesData[b].bind));
		_clipData.assign(_clips.size(), AnimClip{});
		// 기본 클립: 선호 이름(Idle 등) 우선, 없으면 0
		std::wstring pref = FindClipA(_modelDir, _modelStem);
		_curClip = 0;
		for (size_t i = 0; i < _clips.size(); ++i)
			if (_clips[i] == pref) { _curClip = (int)i; break; }
		EnsureNotifies();
		ComputeAndUpload(); _skinnedOnce = true;
	};

	// ── 캐시 히트: 파일/텍스처 재로드 없이 공유 (스폰 즉시) ──
	auto it = s_modelCache.find(meshPath);
	if (it != s_modelCache.end())
	{
		auto& c = *it->second;
		_bonesData = c.bones; _skinSrc = c.skinSrc; _indices = c.indices; _submeshes = c.submeshes;
		_modelScale = c.modelScale; _modelOffset = c.modelOffset; _clips = c.clips;
		_modelDir = c.modelDir; _modelStem = c.modelStem;
		_ib = c.ib; _ibv = c.ibv; _srcVB = c.srcVB; // GPU 리소스 공유 (IB/소스정점)
		_localMin = c.localMin; _localMax = c.localMax;
		_matResources = c.matResources; _whiteTex = c.whiteTex; _srvHeap = c.srvHeap;
		_srvInc = c.srvInc; _matCount = c.matCount; _subMatSlot = c.subMatSlot; _hasTexture = c.hasTexture;
		_vtxCount = (uint32)_skinSrc.size(); _idxCount = (uint32)_indices.size();
		createGpuBufs();
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
	_localMin = mn; _localMax = mx; // 바인드포즈 로컬 AABB (월드 AABB 근사용)
	_vtxCount = (uint32)_skinSrc.size();
	_idxCount = (uint32)_indices.size();

	ScanClips();
	CreateMaterials();

	// 공유 IB (인덱스는 인스턴스 불변)
	const size_t ibSize = (size_t)_idxCount * sizeof(uint32);
	_ib = MakeUploadA(_dev->_device.Get(), ibSize);
	void* p = nullptr; D3D12_RANGE nr2{ 0, 0 }; _ib->Map(0, &nr2, &p); memcpy(p, _indices.data(), ibSize); _ib->Unmap(0, nullptr);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress(); _ibv.Format = DXGI_FORMAT_R32_UINT; _ibv.SizeInBytes = (UINT)ibSize;

	// 공유 소스 정점 (GPU 스키닝 입력 — 바인드포즈, 인스턴스 불변)
	_srcVB = _dev->CreateUploadBuffer(_skinSrc.data(), _skinSrc.size() * sizeof(SkinVtx));

	createGpuBufs();
	finishClip();

	// 캐시에 등록
	auto c = std::make_shared<CachedModel>();
	c->bones = _bonesData; c->skinSrc = _skinSrc; c->indices = _indices;
	c->submeshes = _submeshes; c->modelScale = _modelScale; c->modelOffset = _modelOffset;
	c->clips = _clips; c->modelDir = _modelDir; c->modelStem = _modelStem;
	c->ib = _ib; c->ibv = _ibv; c->srcVB = _srcVB; c->localMin = _localMin; c->localMax = _localMax;
	c->matResources = _matResources; c->whiteTex = _whiteTex; c->srvHeap = _srvHeap;
	c->srvInc = _srvInc; c->matCount = _matCount; c->subMatSlot = _subMatSlot; c->hasTexture = _hasTexture;
	s_modelCache[meshPath] = c;
	return true;
}

// .mmat → 서브메시 슬롯별 디퓨즈/노멀/스펙 텍스처 + 연속 SRV 힙 (ModelScene 미러)
// _slotMats 를 머티리얼 슬롯 수(max(1,_matCount))만큼 기본 머티리얼로 채움 (부족분만 추가)
void ModelAnimator::EnsureSlotMats()
{
	uint32 n = _matCount ? _matCount : 1;
	while (_slotMats.size() < n) _slotMats.push_back(make_shared<Material>());
}

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
	EnsureSlotMats(); // 슬롯별 머티리얼 오버라이드 준비
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
	{ std::vector<uint8_t> flat = { 128,128,255,255 }; _flatNormalTex = makeTex(flat, 1, 1); } // 노멀 슬롯 폴백

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
		_matResources[i] = dec[i].ok ? makeTex(dec[i].px, dec[i].w, dec[i].h)
			: ((i % 3) == 1 ? _flatNormalTex : _whiteTex); // 노멀 슬롯은 평평노멀, 그 외 흰색
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
	_curClip = i; _animTime = 0.f; _prevClip = -1; _fadeDur = 0.f; _prevNorm = 0.f;
	_skinnedOnce = false; // 일시정지 상태에서도 새 포즈 반영
}

void ModelAnimator::Play(int clip, float blend)
{
	if (clip < 0 || clip >= (int)_clips.size() || clip == _curClip) return;
	if (blend > 0.f && _skinnedOnce)
	{
		_prevClip = _curClip; _prevTime = _animTime; _fadeDur = blend; _fadeElapsed = 0.f;
	}
	else _prevClip = -1;
	_curClip = clip; _animTime = 0.f; _prevNorm = 0.f;
	_skinnedOnce = false;
}

const AnimClip* ModelAnimator::ClipData(int i)
{
	if (i < 0 || i >= (int)_clips.size()) return nullptr;
	if ((int)_clipData.size() != (int)_clips.size()) _clipData.assign(_clips.size(), AnimClip{});
	AnimClip& c = _clipData[i];
	if (!c.loaded) { LoadClip(_clips[i], c); c.loaded = true; }
	return &c;
}

float ModelAnimator::ClipDuration(int i)
{
	const AnimClip* c = ClipData(i);
	return (c && c->frameRate > 0.f && c->frameCount > 0) ? c->frameCount / c->frameRate : 0.f;
}

// 클립을 연속 시간(초)으로 샘플 — 인접 프레임 보간(S/T lerp, R slerp). 클립에 없는 본은 단위 포즈.
void ModelAnimator::SamplePose(const AnimClip& clip, float timeSec, bool loop, std::vector<BonePose>& out)
{
	const size_t nb = _bonesData.size();
	out.assign(nb, BonePose{});
	if (clip.frameCount == 0) return;
	const uint32 fc = clip.frameCount;
	float fpos = timeSec * clip.frameRate;
	uint32 f0, f1; float a;
	if (loop)
	{
		float w = fmodf(fpos, (float)fc); if (w < 0.f) w += fc;
		f0 = (uint32)w; a = w - f0; f1 = (f0 + 1) % fc;
	}
	else
	{
		if (fpos >= (float)(fc - 1)) { f0 = f1 = fc - 1; a = 0.f; }
		else { f0 = (uint32)fpos; a = fpos - f0; f1 = f0 + 1; }
	}
	for (size_t b = 0; b < nb; ++b)
	{
		auto it = clip.bones.find(_bonesData[b].name);
		if (it == clip.bones.end() || it->second.empty()) continue;
		const std::vector<ClipFrameT>& fr = it->second;
		uint32 i0 = f0 < fr.size() ? f0 : (uint32)fr.size() - 1;
		uint32 i1 = f1 < fr.size() ? f1 : i0;
		const ClipFrameT& k0 = fr[i0]; const ClipFrameT& k1 = fr[i1];
		BonePose p;
		XMStoreFloat3(&p.s, XMVectorLerp(XMLoadFloat3(&k0.s), XMLoadFloat3(&k1.s), a));
		XMStoreFloat3(&p.t, XMVectorLerp(XMLoadFloat3(&k0.t), XMLoadFloat3(&k1.t), a));
		XMStoreFloat4(&p.r, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&k0.r), XMLoadFloat4(&k1.r), a)));
		out[b] = p;
	}
}

void ModelAnimator::BlendPose(std::vector<BonePose>& a, const std::vector<BonePose>& b, float w)
{
	const size_t n = min(a.size(), b.size());
	for (size_t i = 0; i < n; ++i)
	{
		XMStoreFloat3(&a[i].s, XMVectorLerp(XMLoadFloat3(&a[i].s), XMLoadFloat3(&b[i].s), w));
		XMStoreFloat3(&a[i].t, XMVectorLerp(XMLoadFloat3(&a[i].t), XMLoadFloat3(&b[i].t), w));
		XMStoreFloat4(&a[i].r, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&a[i].r), XMLoadFloat4(&b[i].r), w)));
	}
}

void ModelAnimator::BuildSkinMatrices(const std::vector<BonePose>& local, std::vector<XMMATRIX>& skinOut)
{
	const size_t nb = _bonesData.size();
	_global.resize(nb);
	skinOut.resize(nb);
	for (size_t b = 0; b < nb; ++b)
	{
		const BonePose& p = local[b];
		XMMATRIX m = XMMatrixScaling(p.s.x, p.s.y, p.s.z) *
		             XMMatrixRotationQuaternion(XMLoadFloat4(&p.r)) *
		             XMMatrixTranslation(p.t.x, p.t.y, p.t.z);
		const LoadedBone& bone = _bonesData[b];
		XMMATRIX parent = (bone.parent >= 0 && bone.parent < (int32_t)nb) ? _global[bone.parent] : XMMatrixIdentity();
		_global[b] = m * parent;
		skinOut[b] = _invBind[b] * _global[b]; // 캐시된 역바인드
	}
}

// SkinParams — Skinning.hlsl 의 cbuffer 와 바이트 일치 (row_major 2행렬 + 카운트)
struct SkinParams { Matrix modelToWorld; Matrix world; uint32 vtxCount, boneCount, p0, p1; };

// 현재(+페이드아웃) 클립을 블렌드 → 본 행렬/SkinParams 업로드 + 월드 AABB 근사.
// 실제 정점 변환(스키닝)은 GPU 컴퓨트(RecordSkinning)가 수행. CPU 는 본 행렬(≈수십~백)만 계산.
void ModelAnimator::ComputeAndUpload()
{
	const size_t nb = _bonesData.size();
	const AnimClip* cur = ClipData(_curClip);
	const bool anim = cur && cur->frameCount > 0 && nb > 0;

	// 배치 행렬: 위치=오프셋/스케일/월드 베이크, 방향=월드 회전부
	XMMATRIX M = XMMatrixIdentity();
	if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); M = XMLoadFloat4x4(&wm); }
	XMMATRIX adjust = XMMatrixTranslation(-_modelOffset.x, -_modelOffset.y, -_modelOffset.z) *
	                  XMMatrixScaling(_modelScale, _modelScale, _modelScale);
	XMMATRIX modelToWorld = adjust * M;

	uint32 boneCount = 0;
	if (anim)
	{
		SamplePose(*cur, _animTime, _loop, _poseA);
		if (_prevClip >= 0 && _fadeDur > 0.f)
		{
			const AnimClip* prev = ClipData(_prevClip);
			if (prev && prev->frameCount > 0)
			{
				SamplePose(*prev, _prevTime, true, _poseB);
				float w = _fadeElapsed / _fadeDur; if (w > 1.f) w = 1.f;
				BlendPose(_poseB, _poseA, w); // _poseB = lerp(prev, cur, w)
				_poseA.swap(_poseB);
			}
		}
		BuildSkinMatrices(_poseA, _skin); // XMMATRIX = 행우선 64B = HLSL row_major float4x4
		boneCount = (uint32)nb;
		if (_boneMapped && _boneCap >= boneCount)
			memcpy(_boneMapped, _skin.data(), (size_t)boneCount * sizeof(XMMATRIX));
	}

	// SkinParams 업로드 (boneCount=0 이면 셰이더가 바인드포즈 직접 사용)
	if (_skinCBMapped)
	{
		SkinParams sp{};
		XMStoreFloat4x4(&sp.modelToWorld, modelToWorld);
		XMStoreFloat4x4(&sp.world, M);
		sp.vtxCount = _vtxCount; sp.boneCount = boneCount;
		memcpy(_skinCBMapped, &sp, sizeof(sp));
	}

	// 월드 AABB — 애니 중이면 현재 포즈 본 위치 기준(포즈를 따라 타이트하게), 아니면 바인드포즈 로컬 AABB.
	XMVECTOR mn = XMVectorReplicate(1e9f), mx = XMVectorReplicate(-1e9f);
	if (anim && _global.size() == nb && nb > 0)
	{
		// 본 조인트 월드 위치로 바운드 산출 → 메시 살/손발/머리 여유로 고정 마진(월드 단위 m) 확장.
		// 바인드포즈 T자(팔 벌림) 박스보다 훨씬 타이트하고, 무릎 꿇기 등 포즈 변화도 따라감.
		for (size_t b = 0; b < nb; ++b)
		{
			XMVECTOR p = XMVector3Transform(_global[b].r[3], modelToWorld);
			mn = XMVectorMin(mn, p); mx = XMVectorMax(mx, p);
		}
		XMVECTOR margin = XMVectorSet(0.22f, 0.22f, 0.22f, 0.f);
		XMStoreFloat3(&_aabbMin, XMVectorSubtract(mn, margin));
		XMStoreFloat3(&_aabbMax, XMVectorAdd(mx, margin));
	}
	else
	{
		const XMFLOAT3 lo = _localMin, hi = _localMax;
		for (int c = 0; c < 8; ++c)
		{
			XMVECTOR p = XMVectorSet((c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z, 1.f);
			p = XMVector3Transform(p, modelToWorld);
			mn = XMVectorMin(mn, p); mx = XMVectorMax(mx, p);
		}
		XMVECTOR pad = XMVectorScale(XMVectorSubtract(mx, mn), 0.1f); // 패딩 25%→10% (덜 헐겁게)
		XMStoreFloat3(&_aabbMin, XMVectorSubtract(mn, pad));
		XMStoreFloat3(&_aabbMax, XMVectorAdd(mx, pad));
	}

	_skinDirty = true;
}

// 디버그 스켈레톤 — _global(현재 포즈, 모델 로컬) × modelToWorld 로 본 월드 위치 산출 →
// 부모가 있는 본마다 (부모, 자식) 선분 추가. UpdateWorld 직후 호출(이 프레임 포즈 반영).
void ModelAnimator::GetBoneLines(std::vector<std::pair<XMFLOAT3, XMFLOAT3>>& out)
{
	const size_t nb = _bonesData.size();
	if (nb == 0 || _global.size() < nb) return; // 포즈 미계산(바인드포즈/클립 없음) → 생략

	XMMATRIX M = XMMatrixIdentity();
	if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); M = XMLoadFloat4x4(&wm); }
	XMMATRIX adjust = XMMatrixTranslation(-_modelOffset.x, -_modelOffset.y, -_modelOffset.z) *
	                  XMMatrixScaling(_modelScale, _modelScale, _modelScale);
	XMMATRIX modelToWorld = adjust * M;

	auto bonePos = [&](size_t b) {
		XMVECTOR p = XMVector3Transform(_global[b].r[3], modelToWorld); // _global[b] 의 평행이동행
		XMFLOAT3 f; XMStoreFloat3(&f, p); return f; };

	for (size_t b = 0; b < nb; ++b)
	{
		int parent = _bonesData[b].parent;
		if (parent < 0 || parent >= (int)nb) continue;
		out.emplace_back(bonePos(parent), bonePos(b));
	}
}

// GPU 스키닝 디스패치 — 본 행렬 × 소스정점 → 월드 VB(UAV). 더티일 때만.
void ModelAnimator::RecordSkinning(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || !_vb || _vtxCount == 0) return;

	auto toState = [&](D3D12_RESOURCE_STATES to)
	{
		if (_vbState == to) return;
		D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Transition.pResource = _vb.Get(); b.Transition.StateBefore = _vbState; b.Transition.StateAfter = to;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		cmd->ResourceBarrier(1, &b); _vbState = to;
	};
	auto toPrev = [&](D3D12_RESOURCE_STATES to)
	{
		if (!_vbPrev || _vbPrevState == to) return;
		D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Transition.pResource = _vbPrev.Get(); b.Transition.StateBefore = _vbPrevState; b.Transition.StateAfter = to;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		cmd->ResourceBarrier(1, &b); _vbPrevState = to;
	};

	// 직전 프레임 월드 정점 보존 (속도 G버퍼) — _vb 가 직전 결과(combined read, COPY_SOURCE 포함)일 때 복사.
	// 매프레임 수행: 애니 중이면 prev=직전포즈→objVel=프레임델타, 정지면 prev=cur→objVel=0.
	if (_vbPrev && _vbPrevInit)
	{
		toPrev(D3D12_RESOURCE_STATE_COPY_DEST);
		cmd->CopyResource(_vbPrev.Get(), _vb.Get());
		toPrev(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	if (!_skinDirty) return; // 포즈 변화 없음 — 재스킨 생략(prev 는 위에서 cur 와 동기화)

	toState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmd->SetPipelineState(_dev->_skinPSO.Get());
	cmd->SetComputeRootSignature(_dev->_skinRootSig.Get());
	cmd->SetComputeRootConstantBufferView(0, _skinCB->GetGPUVirtualAddress());
	cmd->SetComputeRootShaderResourceView(1, _srcVB->GetGPUVirtualAddress());
	cmd->SetComputeRootShaderResourceView(2, _boneBuf->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(3, _vb->GetGPUVirtualAddress());
	cmd->Dispatch((_vtxCount + 63) / 64, 1, 1);

	D3D12_RESOURCE_BARRIER uav{}; uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav.UAV.pResource = _vb.Get();
	cmd->ResourceBarrier(1, &uav);

	// 결합 read 상태 — 래스터 VB + BLAS(NON_PIXEL) + 집계 복사(COPY_SOURCE) 동시 유효
	toState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
	        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
	        D3D12_RESOURCE_STATE_COPY_SOURCE);
	_skinDirty = false;

	// 첫 스킨 후 prev 초기화 (prev=cur → 첫 프레임 objVel 0, 이후 프레임부터 정상 델타)
	if (_vbPrev && !_vbPrevInit)
	{
		toPrev(D3D12_RESOURCE_STATE_COPY_DEST);
		cmd->CopyResource(_vbPrev.Get(), _vb.Get());
		toPrev(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_vbPrevInit = true;
	}
}

// 속도 G버퍼 — 현재 월드 VB(드로우) + 직전 프레임 월드 VB(gPrevVB t0, 루트 slot1) → 셰이더가 오브젝트 고유 속도 기록.
void ModelAnimator::RecordVelocity(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || !_vb || !_vbPrev || _vtxCount == 0 || !_vbPrevInit) return;
	cmd->SetGraphicsRootShaderResourceView(1, _vbPrev->GetGPUVirtualAddress());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced(_idxCount, 1, 0, 0, 0);
}

// 상태머신: Any/현재 상태에서 나가는 전이를 평가 → 조건 충족 시 크로스페이드 진입
void ModelAnimator::EvalStateMachine()
{
	if (_states.empty()) return;
	if (_curState < 0) { SetState(0); return; }

	float dur = ClipDuration(_curClip);
	float nt = dur > 0.f ? _animTime / dur : 1.f; // 비루프 정규화 진행도

	auto pass = [&](const AnimTransition& tr) -> bool
	{
		if (tr.hasExitTime && nt < tr.exitTime) return false;
		if (tr.param.empty()) return tr.hasExitTime; // 조건 없음 → ExitTime 도달 시만
		auto it = _params.find(tr.param);
		float v = (it != _params.end()) ? it->second : 0.f;
		switch (tr.op) { case 0: return v > tr.value; case 1: return v < tr.value; default: return v != 0.f; }
	};

	for (const AnimTransition& tr : _transitions)
	{
		if (tr.from != -1 && tr.from != _curState) continue;
		if (tr.to == _curState) continue;
		if (tr.to < 0 || tr.to >= (int)_states.size()) continue;
		if (pass(tr))
		{
			// 트리거(op==2) 소비
			if (!tr.param.empty() && tr.op == 2) _params[tr.param] = 0.f;
			SetState(tr.to, tr.blend);
			break;
		}
	}
}

void ModelAnimator::SetState(int s, float blend)
{
	if (s < 0 || s >= (int)_states.size()) return;
	const AnimState& st = _states[s];
	float b = (_curState >= 0) ? blend : 0.f; // 최초 진입은 즉시
	_loop = st.loop; _speed = st.speed;
	_curState = s;
	Play(st.clip, b);
}

void ModelAnimator::FireNotifies(int clip, float prevNorm, float curNorm)
{
	if (clip < 0 || clip >= (int)_notifies.size()) return;
	const std::vector<AnimNotify>& list = _notifies[clip];
	if (list.empty()) return;
	auto fire = [&](const AnimNotify& n)
	{
		std::string msg = std::string(fs::path(_clips[clip]).stem().string()) + ":" + n.name;
		_eventLog.push_back(msg);
		if (_eventLog.size() > 8) _eventLog.erase(_eventLog.begin());
		if (OnNotify) OnNotify(n.name);
	};
	if (curNorm >= prevNorm) // 정상 진행
	{
		for (const AnimNotify& n : list) if (n.time > prevNorm && n.time <= curNorm) fire(n);
	}
	else // 루프 랩어라운드 (prev..1] ∪ [0..cur]
	{
		for (const AnimNotify& n : list) if (n.time > prevNorm || n.time <= curNorm) fire(n);
	}
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
	const AnimClip* cur = ClipData(_curClip);
	const bool anim = cur && cur->frameCount > 0;

	// 시간 전진 (현재 + 페이드아웃 클립)
	if (anim && _playing)
	{
		_animTime += DT * _speed;
		if (_prevClip >= 0)
		{
			_prevTime += DT; _fadeElapsed += DT;
			if (_fadeElapsed >= _fadeDur) _prevClip = -1;
		}
	}

	// 상태머신 평가 (전이 충족 시 크로스페이드)
	if (_useSM) EvalStateMachine();

	// 노티파이 (현재 클립 정규화 시간 크로싱)
	if (anim)
	{
		float dur = ClipDuration(_curClip);
		float nt = (dur > 0.f) ? _animTime / dur : 0.f;
		float curNorm = _loop ? (nt - floorf(nt)) : (nt > 1.f ? 1.f : nt);
		FireNotifies(_curClip, _prevNorm, curNorm);
		_prevNorm = curNorm;
	}

	// 더티 판단 — 재생/페이드 중이거나 트랜스폼 변경 시에만 재계산
	uint32 ver = 0; if (auto t = GetTransform()) ver = t->Version();
	const bool moving = (anim && _playing) || (_prevClip >= 0);
	if (_skinnedOnce && !moving && ver == _bakedVer) return;

	ComputeAndUpload();
	_bakedVer = ver; _skinnedOnce = true; _blasDirty = true;
}

void ModelAnimator::RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || _vtxCount == 0) return;
	if (_blas && !_blasDirty) return; // 변경 없으면 기존 BLAS 유지
	// 스키닝은 매 프레임 변형 → allowUpdate: 첫 프레임 풀빌드, 이후 in-place refit (풀빌드보다 수배 저렴)
	RtBlas::Build(_dev->_device.Get(), cmd, _vb.Get(), _ib.Get(),
	              _vtxCount, _idxCount, sizeof(Vtx), _blas, _blasScratch, _blasBuilt, true);
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

	// per-submesh 머티리얼 루트상수 — 슬롯별 _slotMats 의 PBR/틴트(인스펙터 편집), 텍스처는 .mmat 슬롯
	EnsureSlotMats();
	auto setMC = [&](uint32 slot, uint32 mode)
	{
		const Material& m = *_slotMats[slot < _slotMats.size() ? slot : 0];
		struct { uint32 mode; float met, rough, emis, tr, tg, tb, pad; } mc{
			mode, m._metallic, m._roughness, m._emissive, m._diffuse.x, m._diffuse.y, m._diffuse.z, 0.f };
		cmd->SetGraphicsRoot32BitConstants(4, 8, &mc, 0);
	};

	if (_hasTexture && !_submeshes.empty())
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		D3D12_GPU_DESCRIPTOR_HANDLE base = _srvHeap->GetGPUDescriptorHandleForHeapStart();
		for (size_t i = 0; i < _submeshes.size(); ++i)
		{
			setMC(_subMatSlot[i], 1u); // 서브메시 슬롯 머티리얼
			D3D12_GPU_DESCRIPTOR_HANDLE hh = base;
			hh.ptr += SIZE_T(_subMatSlot[i]) * 3 * _srvInc; // 슬롯×3 디스크립터
			cmd->SetGraphicsRootDescriptorTable(3, hh);
			cmd->DrawIndexedInstanced(_submeshes[i].indexCount, 1, _submeshes[i].indexStart, 0, 0);
		}
	}
	else
	{
		setMC(0, 2u); // 텍스처 없음 → 정점색 × 틴트 (슬롯 0)
		cmd->DrawIndexedInstanced(_idxCount, 1, 0, 0, 0);
	}
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

// Idle/Run 상태 + Speed 파라미터 + 양방향 전이 자동 구성 (인스펙터 버튼 + 게임 코드 공용)
void ModelAnimator::SetupLocomotion()
{
	auto lower = [](std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c += 32; return s; };
	auto findClip = [&](std::initializer_list<const char*> keys, int fallback) -> int
	{
		for (const char* k : keys)
			for (int i = 0; i < (int)_clips.size(); ++i)
				if (lower(fs::path(_clips[i]).stem().string()).find(k) != std::string::npos) return i;
		return fallback;
	};
	int idle = findClip({ "idle", "stand" }, 0);
	int run  = findClip({ "sprint", "run", "jog", "walk" }, _clips.size() > 1 ? 1 : 0);
	_states.clear(); _transitions.clear();
	_states.push_back({ "Idle", idle, 1.f, true });
	_states.push_back({ "Run",  run,  1.f, true });
	_params["Speed"] = 0.f;
	AnimTransition t1; t1.from = 0; t1.to = 1; t1.param = "Speed"; t1.op = 0; t1.value = 0.1f; t1.blend = 0.15f;
	AnimTransition t2; t2.from = 1; t2.to = 0; t2.param = "Speed"; t2.op = 1; t2.value = 0.1f; t2.blend = 0.20f;
	_transitions.push_back(t1); _transitions.push_back(t2);
	_useSM = true; _curState = -1;
}

void ModelAnimator::OnInspectorGUI()
{
	ImGui::SeparatorText("ModelAnimator");
	ImGui::Text("Mesh: %ls  (%u verts, %u bones)", _modelStem.c_str(), _vtxCount, (uint32)_bonesData.size());

	// \0 구분 클립 이름 콤보 항목 (재사용)
	std::string names;
	for (auto& c : _clips) { names += std::string(fs::path(c).stem().string()); names.push_back('\0'); }
	names.push_back('\0');

	// ── 재생 ──
	if (!_clips.empty())
	{
		ImGui::BeginDisabled(_useSM); // 상태머신 ON 이면 클립은 SM 이 제어
		int idx = _curClip;
		if (ImGui::Combo("Clip", &idx, names.c_str())) Play(idx, _blendDefault); // 크로스페이드 전환
		ImGui::EndDisabled();
		const AnimClip* cd = ClipData(_curClip);
		ImGui::Text("Frames %u  @ %.0f fps  (%.2fs)", cd ? cd->frameCount : 0,
		            cd ? cd->frameRate : 0.f, ClipDuration(_curClip));
		if (_prevClip >= 0)
			ImGui::TextColored(ImVec4(0.6f, 0.85f, 1, 1), "Blending %.0f%%", 100.f * (_fadeDur > 0 ? _fadeElapsed / _fadeDur : 1.f));
	}
	else ImGui::TextDisabled("(no clips)");
	ImGui::Checkbox("Playing", &_playing); ImGui::SameLine();
	ImGui::Checkbox("Loop", &_loop);
	ImGui::DragFloat("Speed", &_speed, 0.02f, 0.f, 4.f);
	ImGui::SliderFloat("Blend Time", &_blendDefault, 0.f, 1.f, "%.2fs");
	HelpMarker("클립 전환 시 크로스페이드(이전 포즈→새 포즈) 시간. 0이면 즉시 컷.");

	// ── 상태머신 (코드 기반 Animator Controller) ──
	if (ImGui::CollapsingHeader("State Machine"))
	{
		ImGui::Checkbox("Use State Machine", &_useSM);
		HelpMarker("켜면 파라미터/전이 규칙으로 클립이 자동 전환됩니다 (게임 로직 주도).");

		// ── 빠른 시작: 흔한 로코모션(Idle/Run + Speed) 자동 구성 ──
		if (ImGui::Button("Auto-setup Locomotion")) SetupLocomotion();
		ImGui::SameLine(); HelpMarker("Idle/Run 상태 + Speed 파라미터 + 전이를 자동 생성합니다.\n아래 Parameters 의 Speed 를 0.1 이상으로 올리면 Run 으로 크로스페이드 전환됩니다.");

		if (_useSM && _curState >= 0 && _curState < (int)_states.size())
			ImGui::TextColored(ImVec4(0.6f, 1, 0.7f, 1), "● Current: %s", _states[_curState].name.c_str());
		else if (_states.empty())
			ImGui::TextDisabled("상태 없음 — 위 버튼으로 시작하거나 아래 + State 로 추가하세요.");

		// 상태 목록 (이름 / 클립 / 속도 / 루프 / ▶미리보기 / 삭제)
		ImGui::SeparatorText("States");
		for (int s = 0; s < (int)_states.size(); ++s)
		{
			ImGui::PushID(s);
			AnimState& st = _states[s];
			char buf[64]; strncpy_s(buf, st.name.c_str(), sizeof(buf) - 1);
			ImGui::SetNextItemWidth(110); if (ImGui::InputText("##n", buf, sizeof(buf))) st.name = buf;
			ImGui::SameLine(); ImGui::SetNextItemWidth(110);
			ImGui::Combo("##c", &st.clip, names.c_str());
			ImGui::SameLine(); ImGui::SetNextItemWidth(60); ImGui::DragFloat("##sp", &st.speed, 0.02f, 0.f, 4.f, "x%.1f");
			ImGui::SameLine(); ImGui::Checkbox("loop", &st.loop);
			ImGui::SameLine(); if (ImGui::SmallButton("\xE2\x96\xB6")) { Play(st.clip, _blendDefault); } // ▶ 이 상태 클립 미리보기
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("이 상태의 클립을 즉시 재생(미리보기)");
			ImGui::SameLine(); if (ImGui::SmallButton("X")) { _states.erase(_states.begin() + s); ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (ImGui::SmallButton("+ State")) _states.push_back({ "State" + std::to_string(_states.size()), _curClip, 1.f, true });

		// 전이 목록
		ImGui::SeparatorText("Transitions");
		const char* ops[] = { ">", "<", "!=0 (trigger)" };
		auto stateName = [&](int i) { return (i < 0) ? "Any" : (i < (int)_states.size() ? _states[i].name.c_str() : "?"); };
		for (int t = 0; t < (int)_transitions.size(); ++t)
		{
			ImGui::PushID(1000 + t);
			AnimTransition& tr = _transitions[t];
			ImGui::SetNextItemWidth(80);
			if (ImGui::BeginCombo("##from", stateName(tr.from)))
			{
				if (ImGui::Selectable("Any", tr.from == -1)) tr.from = -1;
				for (int s = 0; s < (int)_states.size(); ++s) if (ImGui::Selectable(_states[s].name.c_str(), tr.from == s)) tr.from = s;
				ImGui::EndCombo();
			}
			ImGui::SameLine(); ImGui::TextUnformatted("->"); ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			if (ImGui::BeginCombo("##to", stateName(tr.to)))
			{
				for (int s = 0; s < (int)_states.size(); ++s) if (ImGui::Selectable(_states[s].name.c_str(), tr.to == s)) tr.to = s;
				ImGui::EndCombo();
			}
			char pbuf[48]; strncpy_s(pbuf, tr.param.c_str(), sizeof(pbuf) - 1);
			ImGui::SetNextItemWidth(80); if (ImGui::InputTextWithHint("##p", "param", pbuf, sizeof(pbuf))) tr.param = pbuf;
			ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::Combo("##op", &tr.op, ops, 3);
			if (tr.op != 2) { ImGui::SameLine(); ImGui::SetNextItemWidth(60); ImGui::DragFloat("##v", &tr.value, 0.05f); }
			ImGui::SameLine(); ImGui::SetNextItemWidth(60); ImGui::DragFloat("##bl", &tr.blend, 0.01f, 0.f, 1.f, "%.2fs");
			ImGui::SameLine(); ImGui::Checkbox("exit", &tr.hasExitTime);
			ImGui::SameLine(); if (ImGui::SmallButton("X")) { _transitions.erase(_transitions.begin() + t); ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (ImGui::SmallButton("+ Transition")) _transitions.push_back({});

		// 파라미터 (라이브 제어/테스트)
		ImGui::SeparatorText("Parameters");
		for (auto& kv : _params)
		{
			ImGui::PushID(kv.first.c_str());
			ImGui::SetNextItemWidth(140); ImGui::DragFloat(kv.first.c_str(), &kv.second, 0.05f);
			ImGui::SameLine(); if (ImGui::SmallButton("Set 1")) kv.second = 1.f;
			ImGui::SameLine(); if (ImGui::SmallButton("0")) kv.second = 0.f;
			ImGui::PopID();
		}
		static char newParam[48] = "";
		ImGui::SetNextItemWidth(140); ImGui::InputTextWithHint("##np", "new param", newParam, sizeof(newParam));
		ImGui::SameLine(); if (ImGui::SmallButton("+ Param") && newParam[0]) { _params[newParam] = 0.f; newParam[0] = 0; }
	}

	// ── 노티파이 (현재 클립에 이벤트 마커) + 이벤트 로그 ──
	if (!_clips.empty() && ImGui::CollapsingHeader("Notifies & Events"))
	{
		EnsureNotifies();
		ImGui::Text("Clip: %s", std::string(fs::path(_clips[_curClip]).stem().string()).c_str());
		std::vector<AnimNotify>& list = _notifies[_curClip];
		for (int i = 0; i < (int)list.size(); ++i)
		{
			ImGui::PushID(2000 + i);
			char nb[48]; strncpy_s(nb, list[i].name.c_str(), sizeof(nb) - 1);
			ImGui::SetNextItemWidth(120); if (ImGui::InputTextWithHint("##nm", "event", nb, sizeof(nb))) list[i].name = nb;
			ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::SliderFloat("##tm", &list[i].time, 0.f, 1.f, "t=%.2f");
			ImGui::SameLine(); if (ImGui::SmallButton("X")) { list.erase(list.begin() + i); ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (ImGui::SmallButton("+ Notify")) list.push_back({ 0.5f, "event" });
		HelpMarker("정규화 시간(0~1)에 도달하면 이벤트 발생. 게임은 OnNotify 콜백 또는 RecentEvents() 로 수신.");

		ImGui::SeparatorText("Recent Events");
		if (_eventLog.empty()) ImGui::TextDisabled("(none)");
		for (auto it = _eventLog.rbegin(); it != _eventLog.rend(); ++it)
			ImGui::BulletText("%s", it->c_str());
	}

	// ── 머티리얼 (서브메시 슬롯별 Element — 틴트는 .mmat 텍스처에 곱해짐) ──
	EnsureSlotMats();
	ImGui::SeparatorText("Materials");
	ImGui::TextDisabled("Elements: %u (서브메시 머티리얼별 PBR/틴트)", MaterialSlotCount());
	for (uint32 e = 0; e < (uint32)_slotMats.size(); ++e)
	{
		auto& mat = _slotMats[e];
		ImGui::PushID((int)e);
		char hdr[32]; snprintf(hdr, sizeof(hdr), "Element %u", e);
		if (ImGui::CollapsingHeader(hdr, e == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
		{
			ImGui::ColorEdit3("Tint", &mat->_diffuse.x);
			ImGui::DragFloat("Metallic", &mat->_metallic, 0.01f, 0.f, 1.f);
			ImGui::DragFloat("Roughness", &mat->_roughness, 0.01f, 0.f, 1.f);
			ImGui::DragFloat("Emissive", &mat->_emissive, 0.02f, 0.f, 16.f);
			auto preset = [&](const char* n, Vec3 d, float m, float r) { if (ImGui::SmallButton(n)) { mat->_diffuse = d; mat->_metallic = m; mat->_roughness = r; } ImGui::SameLine(); };
			preset("Default", { 1,1,1 }, 0.f, 0.5f);
			preset("Metal", { 0.9f,0.9f,0.9f }, 1.f, 0.25f);
			preset("Gold", { 1.0f,0.78f,0.34f }, 1.f, 0.2f);
			if (ImGui::SmallButton("Plastic")) { mat->_diffuse = { 1,1,1 }; mat->_metallic = 0.f; mat->_roughness = 0.4f; }
			if (_dev) MaterialSlotGUI(_dev->_assetRoot, mat, [this, e](shared_ptr<Material> m) { SetSlotMaterial(e, m); });
		}
		ImGui::PopID();
	}
	ImGui::TextDisabled("(텍스처는 .mmat 슬롯, 여기선 PBR/틴트만)");
}
