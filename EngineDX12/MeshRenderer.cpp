#include "MeshRenderer.h"
#include "D3D12Device.h"
#include "GameObject.h"
#include "Transform.h"
#include "TextureLoader.h"
#include "GeometryHelper.h"
#include "RtBlas.h"
#include "imgui.h"

using namespace DirectX;

static ComPtr<ID3D12Resource> MakeUpload(ID3D12Device* dev, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "MeshRenderer buffer");
	return buf;
}

void MeshRenderer::SetGeometry(const vector<Vtx>& verts, const vector<uint32>& indices)
{
	if (!_dev) return;
	_local = verts; _indices = indices; _world = verts;

	const size_t vbSize = verts.size() * sizeof(Vtx);
	const size_t ibSize = indices.size() * sizeof(uint32);

	_vb = MakeUpload(_dev->_device.Get(), vbSize);
	D3D12_RANGE nr{ 0, 0 }; _vb->Map(0, &nr, &_vbMapped);
	memcpy(_vbMapped, verts.data(), vbSize);
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress(); _vbv.StrideInBytes = sizeof(Vtx); _vbv.SizeInBytes = (UINT)vbSize;

	_ib = MakeUpload(_dev->_device.Get(), ibSize);
	void* p = nullptr; _ib->Map(0, &nr, &p); memcpy(p, indices.data(), ibSize); _ib->Unmap(0, nullptr);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress(); _ibv.Format = DXGI_FORMAT_R32_UINT; _ibv.SizeInBytes = (UINT)ibSize;
}

void MeshRenderer::SetTexture(const std::wstring& path)
{
	std::vector<uint8_t> px; uint32 w = 0, h = 0;
	if (LoadImageRGBA(path, px, w, h) && w && h) SetTexturePixels(px, w, h);
}

// 디퓨즈 텍스처 + 흰색 폴백(노멀/스펙) → 3-SRV 힙. 임시 커맨드리스트 업로드 + 펜스 대기.
void MeshRenderer::SetTexturePixels(const vector<uint8_t>& rgba, uint32 w, uint32 h)
{
	if (!_dev || rgba.empty()) return;
	ID3D12Device5* dev = _dev->_device.Get();

	auto makeTex = [&](uint32 tw, uint32 th)
	{
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC td{}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		td.Width = tw; td.Height = th; td.DepthOrArraySize = 1; td.MipLevels = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
		ComPtr<ID3D12Resource> t;
		ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&t)), "MeshRenderer tex");
		return t;
	};
	_tex = makeTex(w, h);
	_white = makeTex(1, 1);

	ComPtr<ID3D12CommandAllocator> al; ComPtr<ID3D12GraphicsCommandList> cl;
	dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
	dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al.Get(), nullptr, IID_PPV_ARGS(&cl));

	std::vector<ComPtr<ID3D12Resource>> uploads;
	auto upload = [&](ComPtr<ID3D12Resource>& tex, const uint8_t* px, uint32 tw, uint32 th)
	{
		D3D12_RESOURCE_DESC td = tex->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
		dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);
		ComPtr<ID3D12Resource> up = MakeUpload(dev, (size_t)total);
		uint8_t* m = nullptr; D3D12_RANGE nr{ 0, 0 }; up->Map(0, &nr, (void**)&m);
		for (UINT y = 0; y < rows; ++y)
			memcpy(m + fp.Offset + (size_t)y * fp.Footprint.RowPitch, px + (size_t)y * tw * 4, (size_t)tw * 4);
		up->Unmap(0, nullptr);
		uploads.push_back(up);
		D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = tex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
		D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = up.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
		cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex.Get();
		b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; cl->ResourceBarrier(1, &b);
	};
	const uint8_t wpx[4] = { 255,255,255,255 };
	upload(_tex, rgba.data(), w, h);
	upload(_white, wpx, 1, 1);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() };
	_dev->_queue->ExecuteCommandLists(1, lists);

	ComPtr<ID3D12Fence> fence; dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_dev->_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); }
	CloseHandle(ev);

	// 3-SRV 힙 (디퓨즈 / 노멀(흰) / 스펙(흰)) — 메인 셰이더 테이블 t2/t3/t4 레이아웃
	D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 3; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "MeshRenderer srv heap");
	_srvInc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_SHADER_RESOURCE_VIEW_DESC sd{}; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sd.Texture2D.MipLevels = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	dev->CreateShaderResourceView(_tex.Get(), &sd, hcpu);   hcpu.ptr += _srvInc;
	dev->CreateShaderResourceView(_white.Get(), &sd, hcpu); hcpu.ptr += _srvInc;
	dev->CreateShaderResourceView(_white.Get(), &sd, hcpu);
	_hasTex = true;
}

void MeshRenderer::TransformBoundingBox()
{
	// 월드 베이크된 정점 AABB (Draw 에서 _world 갱신됨). 비어 있으면 트랜스폼 위치 기준.
	if (_world.empty()) { Renderer::TransformBoundingBox(); return; }
	XMVECTOR mn = XMVectorReplicate(1e30f), mx = XMVectorReplicate(-1e30f);
	for (auto& v : _world) { XMVECTOR p = XMLoadFloat3(&v.pos); mn = XMVectorMin(mn, p); mx = XMVectorMax(mx, p); }
	XMVECTOR c = XMVectorScale(XMVectorAdd(mn, mx), 0.5f), e = XMVectorScale(XMVectorSubtract(mx, mn), 0.5f);
	XMStoreFloat3(&_boundingBox.Center, c);
	XMStoreFloat3(&_boundingBox.Extents, e);
}

// 트랜스폼이 바뀐 프레임에만 월드 정점 재생성 (효율 — 정적 메시는 1회). gMVP=VP 와 일치.
void MeshRenderer::Rebake()
{
	XMMATRIX W = XMMatrixIdentity();
	if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); W = XMLoadFloat4x4(&wm); }
	for (size_t i = 0; i < _local.size(); ++i)
	{
		XMVECTOR p = XMVector3Transform(XMLoadFloat3(&_local[i].pos), W);
		XMVECTOR n = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&_local[i].nrm), W));
		XMVECTOR tn = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&_local[i].tan), W));
		_world[i] = _local[i];
		XMStoreFloat3(&_world[i].pos, p);
		XMStoreFloat3(&_world[i].nrm, n);
		XMStoreFloat3(&_world[i].tan, tn);
		_world[i].col = XMFLOAT3(_local[i].col.x * _material._diffuse.x,
		                         _local[i].col.y * _material._diffuse.y,
		                         _local[i].col.z * _material._diffuse.z); // 머티리얼 틴트
	}
	memcpy(_vbMapped, _world.data(), _world.size() * sizeof(Vtx));
}

// 머티리얼 디퓨즈 텍스처 경로가 바뀌면 자동으로 SRV 재로드 (인스펙터 편집/씬 로드 반영)
void MeshRenderer::SyncMaterialTex()
{
	if (_material._diffuseTex == _loadedTexPath) return;
	_loadedTexPath = _material._diffuseTex; // 실패해도 갱신(재시도 폭주 방지)
	if (!_material._diffuseTex.empty())
		SetTexture(_material._diffuseTex);
	else
		_hasTex = false; // 경로 비우면 정점색으로 복귀
}

void MeshRenderer::OnInspectorGUI()
{
	bool tintChanged = false;
	ImGui::SeparatorText("MeshRenderer");

	// 프리미티브 교체 (절차적 지오메트리 재생성)
	const char* prims[] = { "(custom)", "Cube", "Sphere", "Plane" };
	int pk = (int)_prim;
	if (ImGui::Combo("Primitive", &pk, prims, 4) && pk != 0 && _dev)
	{
		_prim = (MeshPrim)pk;
		vector<Vtx> v; vector<uint32> idx;
		switch (_prim) {
		case MeshPrim::Sphere: GeometryHelper::CreateSphere(v, idx, 0.5f); break;
		case MeshPrim::Plane:  GeometryHelper::CreatePlane(v, idx, 2.0f);  break;
		default:               GeometryHelper::CreateCube(v, idx, 1.0f);   break;
		}
		SetGeometry(v, idx); _baked = false;
	}

	tintChanged |= ImGui::ColorEdit3("Diffuse (tint)", &_material._diffuse.x);
	ImGui::DragFloat("Metallic", &_material._metallic, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Roughness", &_material._roughness, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Emissive", &_material._emissive, 0.02f, 0.f, 16.f);

	// 디퓨즈 텍스처 경로 — 입력 후 Load 로 적용 (Draw 에서 SyncMaterialTex 가 SRV 생성)
	if (_texPathBuf[0] == '\0' && !_material._diffuseTex.empty())
	{
		int n = WideCharToMultiByte(CP_UTF8, 0, _material._diffuseTex.c_str(), -1, _texPathBuf, sizeof(_texPathBuf), nullptr, nullptr);
		(void)n;
	}
	ImGui::InputText("Diffuse Tex", _texPathBuf, sizeof(_texPathBuf));
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, _texPathBuf, -1, nullptr, 0);
		std::wstring wp(n > 0 ? n - 1 : 0, L'\0');
		if (n > 0) MultiByteToWideChar(CP_UTF8, 0, _texPathBuf, -1, wp.data(), n);
		_material._diffuseTex = wp; // 다음 Draw 의 SyncMaterialTex 가 SRV 로드
	}
	if (_hasTex) { ImGui::SameLine(); if (ImGui::Button("Clear Tex")) { _material._diffuseTex.clear(); _texPathBuf[0] = '\0'; } }

	if (tintChanged) _baked = false; // 틴트는 정점색에 베이크 → 재베이크 강제
}

// RT 통합 — AS 패스에서 호출(Draw 전). 트랜스폼 변경 시에만 월드 정점 재생성.
void MeshRenderer::UpdateWorld()
{
	if (!_dev || _local.empty()) return;
	SyncMaterialTex();
	uint32 ver = 0; if (auto t = GetTransform()) ver = t->Version();
	if (!_baked || ver != _bakedVer) { Rebake(); _baked = true; _bakedVer = ver; }
}

void MeshRenderer::RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || _local.empty()) return;
	RtBlas::Build(_dev->_device.Get(), cmd, _vb.Get(), _ib.Get(),
	              (UINT)_local.size(), (UINT)_indices.size(), sizeof(Vtx), _blas, _blasScratch);
}

void MeshRenderer::Draw(const RenderContext& ctx)
{
	if (!_dev || _local.empty()) return;

	UpdateWorld(); // 더티 아니면 no-op (AS 패스 미수집 오브젝트 안전망)

	D3D12Device& d = *_dev;
	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, d._cb[d._frameIndex]->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(1, d._scene._tlas->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr());
	if (_hasTex)
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetGraphicsRoot32BitConstant(4, 1u, 0); // useTex=1
		cmd->SetGraphicsRootDescriptorTable(3, _srvHeap->GetGPUDescriptorHandleForHeapStart());
	}
	else
		cmd->SetGraphicsRoot32BitConstant(4, 0u, 0); // useTex=0 (정점색)
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced((UINT)_indices.size(), 1, 0, 0, 0);
}
