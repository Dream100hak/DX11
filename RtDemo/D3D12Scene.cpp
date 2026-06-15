#include "D3D12Device.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

using namespace DirectX;

// ───────────────────────────────────────────────────────────
// 씬 리소스 — 모델/바닥 지오메트리, CPU 스키닝, 디퓨즈 텍스처
// (D3D12Device.cpp 에서 분리)
// ───────────────────────────────────────────────────────────

void D3D12Device::CreateCubeGeometry()
{
	std::vector<uint32_t> indices;
	const XMFLOAT3 modelC(0.82f, 0.78f, 0.72f);

	// ── 스키닝 .mesh 모델 로드 (본+블렌드) + .clip 애니메이션 ──
	{
		wchar_t exe[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring dir(exe);
		dir = dir.substr(0, dir.find_last_of(L"\\/"));
		std::wstring base = dir + L"\\..\\Resources\\Assets\\Models\\Kachujin\\";

		if (LoadMeshSkinned(base + L"Kachujin.mesh", _bonesData, _skinSrc, indices))
		{
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
				                                (v.pos.z - _modelOffset.z) * _modelScale), v.nrm, modelC, v.uv });

			_animated = LoadClip(base + L"Idle.clip", _clip);
		}
	}

	_modelIndexCount = (uint32)indices.size(); // 바닥 추가 전 = 모델 인덱스 수

	// ── 바닥 평면 (빨강) — 모델 정점 뒤에 추가 ──
	{
		const float g = 6.f;
		const XMFLOAT3 fc(0.90f, 0.12f, 0.10f), n(0, 1, 0);
		const XMFLOAT2 z(0, 0);
		uint32 b = (uint32)_cpuVerts.size();
		_cpuVerts.push_back({ XMFLOAT3(-g,0, g), n, fc, z }); _cpuVerts.push_back({ XMFLOAT3(g,0, g), n, fc, z });
		_cpuVerts.push_back({ XMFLOAT3( g,0,-g), n, fc, z }); _cpuVerts.push_back({ XMFLOAT3(-g,0,-g), n, fc, z });
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

// 매 프레임 CPU 스키닝 — 본 행렬 계산 → 정점 변형 → VB(영속 매핑) 갱신
void D3D12Device::UpdateAnimation()
{
	if (!_animated || _clip.frameCount == 0) return;

	uint32 frame = (uint32)(_time * _clip.frameRate) % _clip.frameCount;
	size_t nb = _bonesData.size();

	std::vector<XMMATRIX> global(nb), skin(nb);
	for (size_t b = 0; b < nb; ++b)
	{
		const LoadedBone& bone = _bonesData[b];
		XMMATRIX matAnim = XMMatrixIdentity();
		auto it = _clip.bones.find(bone.name);
		if (it != _clip.bones.end() && frame < it->second.size())
		{
			const ClipFrameT& k = it->second[frame];
			XMMATRIX S = XMMatrixScaling(k.s.x, k.s.y, k.s.z);
			XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&k.r));
			XMMATRIX T = XMMatrixTranslation(k.t.x, k.t.y, k.t.z);
			matAnim = S * R * T;
		}
		XMMATRIX parent = (bone.parent >= 0 && bone.parent < (int32_t)nb) ? global[bone.parent] : XMMatrixIdentity();
		global[b] = matAnim * parent;
		XMMATRIX bind = XMLoadFloat4x4(&bone.bind);
		skin[b] = XMMatrixInverse(nullptr, bind) * global[b];
	}

	XMVECTOR off = XMVectorSet(_modelOffset.x, _modelOffset.y, _modelOffset.z, 0.f);
	for (uint32 i = 0; i < _modelVtxCount; ++i)
	{
		const SkinVtx& sv = _skinSrc[i];
		XMVECTOR bp = XMLoadFloat3(&sv.pos), bn = XMLoadFloat3(&sv.nrm);
		XMVECTOR p = XMVectorZero(), n = XMVectorZero();
		float wsum = 0.f;
		for (int j = 0; j < 4; ++j)
		{
			float w = sv.wgt[j];
			uint32 bi = sv.idx[j];
			if (w <= 0.f || bi >= nb) continue;
			p = XMVectorAdd(p, XMVectorScale(XMVector3Transform(bp, skin[bi]), w));
			n = XMVectorAdd(n, XMVectorScale(XMVector3TransformNormal(bn, skin[bi]), w));
			wsum += w;
		}
		if (wsum < 1e-4f) { p = bp; n = bn; } // 미리깅 정점 폴백

		XMVECTOR wp = XMVectorScale(XMVectorSubtract(p, off), _modelScale);
		XMStoreFloat3(&_cpuVerts[i].pos, wp);
		XMStoreFloat3(&_cpuVerts[i].nrm, XMVector3Normalize(n));
	}
	memcpy(_vbMapped, _cpuVerts.data(), _cpuVerts.size() * sizeof(Vtx));
}

void D3D12Device::CreateTextureResources()
{
	wchar_t exe[MAX_PATH]{};
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe);
	dir = dir.substr(0, dir.find_last_of(L"\\/"));
	std::wstring path = dir + L"\\..\\Resources\\Assets\\Models\\Kachujin\\Kachujin_diffuse.png";

	std::vector<uint8_t> px; uint32 tw = 0, th = 0;
	if (!LoadImageRGBA(path, px, tw, th)) { _hasTexture = false; return; }

	// 디퓨즈 텍스처 (DEFAULT, COPY_DEST)
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC td{};
	td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	td.Width = tw; td.Height = th; td.DepthOrArraySize = 1; td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_diffuseTex)), "Create Tex");

	// 업로드 버퍼 (행 피치 256 정렬)
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT numRows = 0; UINT64 rowSize = 0, uploadSize = 0;
	_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &numRows, &rowSize, &uploadSize);
	_texUpload = CreateUploadBuffer(nullptr, (size_t)uploadSize);
	uint8_t* mapped = nullptr; D3D12_RANGE noRead{ 0, 0 };
	ThrowIfFailed(_texUpload->Map(0, &noRead, (void**)&mapped), "Map texUpload");
	for (UINT y = 0; y < numRows; ++y)
		memcpy(mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, px.data() + (size_t)y * tw * 4, (size_t)tw * 4);
	_texUpload->Unmap(0, nullptr);

	// 복사 명령 실행
	ThrowIfFailed(_allocators[_frameIndex]->Reset(), "tex alloc reset");
	ThrowIfFailed(_cmdList->Reset(_allocators[_frameIndex].Get(), nullptr), "tex cmd reset");
	D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = _diffuseTex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = _texUpload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
	_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = _diffuseTex.Get();
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &b);
	ThrowIfFailed(_cmdList->Close(), "tex cmd close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();

	// 셰이더 가시 SRV 힙
	D3D12_DESCRIPTOR_HEAP_DESC hd{};
	hd.NumDescriptors = 1;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "srv heap");
	D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sd.Texture2D.MipLevels = 1;
	_device->CreateShaderResourceView(_diffuseTex.Get(), &sd, _srvHeap->GetCPUDescriptorHandleForHeapStart());
	_hasTexture = true;
}
