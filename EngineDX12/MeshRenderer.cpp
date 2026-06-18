#include "MeshRenderer.h"
#include "D3D12Device.h"
#include "GameObject.h"
#include "Transform.h"
#include "TextureLoader.h"
#include "GeometryHelper.h"
#include "RtBlas.h"
#include "ResourceManager.h"
#include "imgui.h"
#include <filesystem>

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

	// 정점 수가 바뀌면 BLAS 버퍼도 새 크기로 재생성해야 함(안 그러면 작은 버퍼에 빌드→GPU 손상/깜빡임)
	_blas.Reset(); _blasScratch.Reset(); _blasDirty = true;
	_baked = false;
}

// 정점 수가 같으면 버퍼 재생성 없이 _local 만 교체 → 다음 Draw 의 Rebake 가 in-place 업로드(스컬프트 효율)
void MeshRenderer::UpdateVertices(const vector<Vtx>& verts)
{
	if (!_dev || verts.size() != _local.size() || !_vb) { SetGeometry(verts, _indices); return; }
	_local = verts;
	_baked = false;       // 다음 UpdateWorld 에서 Rebake(월드 정점 재생성 + memcpy)
	_blasDirty = true;    // RT BLAS 재빌드
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
		_world[i].col = _local[i].col; // 틴트는 셰이더 gObjTint 로 (베이크 안 함)
	}
	memcpy(_vbMapped, _world.data(), _world.size() * sizeof(Vtx));
}

// 머티리얼 디퓨즈 텍스처 경로가 바뀌면 자동으로 SRV 재로드 (인스펙터 편집/씬 로드 반영)
void MeshRenderer::SyncMaterialTex()
{
	if (_material->_diffuseTex == _loadedTexPath) return;
	_loadedTexPath = _material->_diffuseTex; // 실패해도 갱신(재시도 폭주 방지)
	if (!_material->_diffuseTex.empty())
		SetTexture(_material->_diffuseTex);
	else
		_hasTex = false; // 경로 비우면 정점색으로 복귀
}

void MeshRenderer::OnInspectorGUI()
{
	bool tintChanged = false;
	ImGui::SeparatorText("MeshRenderer");

	// 프리미티브 교체 (절차적 지오메트리 재생성)
	const char* prims[] = { "(custom)", "Cube", "Sphere", "Plane", "Cylinder", "Cone", "Torus", "Capsule" };
	int pk = (int)_prim;
	if (ImGui::Combo("Primitive", &pk, prims, IM_ARRAYSIZE(prims)) && pk != 0 && _dev)
	{
		_prim = (MeshPrim)pk;
		vector<Vtx> v; vector<uint32> idx;
		switch (_prim) {
		case MeshPrim::Sphere:   GeometryHelper::CreateSphere(v, idx, 0.5f); break;
		case MeshPrim::Plane:    GeometryHelper::CreatePlane(v, idx, 2.0f);  break;
		case MeshPrim::Cylinder: GeometryHelper::CreateCylinder(v, idx, 0.5f, 1.0f); break;
		case MeshPrim::Cone:     GeometryHelper::CreateCone(v, idx, 0.5f, 1.0f); break;
		case MeshPrim::Torus:    GeometryHelper::CreateTorus(v, idx, 0.35f, 0.15f); break;
		case MeshPrim::Capsule:  GeometryHelper::CreateCapsule(v, idx, 0.35f, 0.6f); break;
		default:                 GeometryHelper::CreateCube(v, idx, 1.0f);   break;
		}
		SetGeometry(v, idx); _baked = false;
	}

	tintChanged |= ImGui::ColorEdit3("Diffuse (tint)", &_material->_diffuse.x);
	ImGui::DragFloat("Metallic", &_material->_metallic, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Roughness", &_material->_roughness, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("Emissive", &_material->_emissive, 0.02f, 0.f, 16.f);
	// PBR 프리셋 (빠른 머티리얼 세팅)
	auto preset = [&](const char* n, Vec3 d, float m, float r) { if (ImGui::SmallButton(n)) { _material->_diffuse = d; _material->_metallic = m; _material->_roughness = r; } ImGui::SameLine(); };
	preset("Metal", { 0.9f,0.9f,0.9f }, 1.f, 0.25f);
	preset("Gold", { 1.0f,0.78f,0.34f }, 1.f, 0.2f);
	preset("Plastic", { 0.8f,0.2f,0.2f }, 0.f, 0.4f);
	preset("Rubber", { 0.15f,0.15f,0.15f }, 0.f, 0.9f);
	if (ImGui::SmallButton("Mirror")) { _material->_diffuse = { 1,1,1 }; _material->_metallic = 1.f; _material->_roughness = 0.02f; }

	// 디퓨즈 텍스처 경로 — 입력 후 Load 로 적용 (Draw 에서 SyncMaterialTex 가 SRV 생성)
	if (_texPathBuf[0] == '\0' && !_material->_diffuseTex.empty())
	{
		int n = WideCharToMultiByte(CP_UTF8, 0, _material->_diffuseTex.c_str(), -1, _texPathBuf, sizeof(_texPathBuf), nullptr, nullptr);
		(void)n;
	}
	ImGui::InputText("Diffuse Tex", _texPathBuf, sizeof(_texPathBuf));
	ImGui::SameLine();
	if (ImGui::Button("Load##tex"))
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, _texPathBuf, -1, nullptr, 0);
		std::wstring wp(n > 0 ? n - 1 : 0, L'\0');
		if (n > 0) MultiByteToWideChar(CP_UTF8, 0, _texPathBuf, -1, wp.data(), n);
		_material->_diffuseTex = wp; // 다음 Draw 의 SyncMaterialTex 가 SRV 로드
	}
	if (_hasTex) { ImGui::SameLine(); if (ImGui::Button("Clear Tex")) { _material->_diffuseTex.clear(); _texPathBuf[0] = '\0'; } }

	// ── .mat 자산(공유) ── 같은 .mat 로드 시 인스턴스끼리 공유 → 편집 일괄 반영
	ImGui::Separator();
	ImGui::TextDisabled(_material->_path.empty() ? "Material: (inline)" : "Material asset (shared)");

	// 유니티/언리얼식 머티리얼 슬롯 — 이름 표시 + 드롭 타겟(MAT_PATH) + 피커 팝업
	{
		auto wToU = [](const std::wstring& w) { if (w.empty()) return std::string(); int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr); std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr); return s; };
		auto uToW = [](const char* u) { int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0'); if (n > 0) MultiByteToWideChar(CP_UTF8, 0, u, -1, w.data(), n); return w; };
		auto assign = [&](const std::wstring& wp) { if (wp.empty()) return; auto sh = GET_SINGLE(ResourceManager)->Get<Material>(wp); if (!sh) { sh = LoadMaterial(wp); if (sh) GET_SINGLE(ResourceManager)->Add<Material>(wp, sh); } if (sh) SetMaterialRef(sh); };

		std::string slot = _material->_path.empty() ? "(inline material)" : wToU(std::filesystem::path(_material->_path).stem().wstring());
		ImGui::Button(("Mat: " + slot).c_str(), ImVec2(-72, 0)); // 슬롯(드롭 타겟)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MAT_PATH")) assign(uToW((const char*)pl->Data));
			ImGui::EndDragDropTarget();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(".mat 을 여기로 드래그하거나 Pick 으로 선택");
		ImGui::SameLine();
		if (ImGui::Button("Pick##matslot")) ImGui::OpenPopup("matpick");
		if (ImGui::BeginPopup("matpick"))
		{
			ImGui::TextDisabled("Materials"); ImGui::Separator();
			int shown = 0;
			if (_dev)
			{
				namespace fs = std::filesystem; std::error_code ec;
				for (auto it = fs::recursive_directory_iterator(_dev->_assetRoot, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
				{
					if (!it->is_regular_file(ec) || it->path().extension() != L".mat") continue;
					std::string nm = wToU(it->path().filename().wstring());
					bool sel = (it->path().wstring() == _material->_path);
					if (ImGui::Selectable(nm.c_str(), sel)) { assign(it->path().wstring()); ImGui::CloseCurrentPopup(); }
					++shown;
				}
			}
			if (shown == 0) ImGui::TextDisabled("(no .mat assets found)");
			ImGui::EndPopup();
		}
	}

	if (_matPathBuf[0] == '\0' && !_material->_path.empty())
		WideCharToMultiByte(CP_UTF8, 0, _material->_path.c_str(), -1, _matPathBuf, sizeof(_matPathBuf), nullptr, nullptr);
	ImGui::InputText(".mat path", _matPathBuf, sizeof(_matPathBuf));
	auto bufToW = [&]() { int n = MultiByteToWideChar(CP_UTF8, 0, _matPathBuf, -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0'); if (n > 0) MultiByteToWideChar(CP_UTF8, 0, _matPathBuf, -1, w.data(), n); return w; };
	if (ImGui::Button("Save .mat"))
	{
		std::wstring wp = bufToW();
		// 디렉터리 구분자 없으면 Assets/Materials/<이름>.mat 로 (작업폴더 오염 방지)
		if (!wp.empty() && wp.find(L'\\') == std::wstring::npos && wp.find(L'/') == std::wstring::npos && _dev)
		{
			std::wstring dir = _dev->_assetRoot + L"\\Materials";
			std::error_code ec; std::filesystem::create_directories(dir, ec);
			if (wp.size() < 5 || wp.substr(wp.size() - 4) != L".mat") wp += L".mat";
			wp = dir + L"\\" + wp;
		}
		if (!wp.empty() && SaveMaterial(*_material, wp))
		{
			_material->_path = wp;
			GET_SINGLE(ResourceManager)->Add<Material>(wp, _material); // 공유 캐시 등록
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Load .mat"))
	{
		std::wstring wp = bufToW();
		if (!wp.empty())
		{
			auto shared = GET_SINGLE(ResourceManager)->Get<Material>(wp);
			if (!shared) { shared = LoadMaterial(wp); if (shared) GET_SINGLE(ResourceManager)->Add<Material>(wp, shared); }
			if (shared) SetMaterialRef(shared); // 공유 인스턴스 참조 → 편집 일괄 반영
		}
	}
	if (!_material->_path.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button("Make Unique")) { auto u = make_shared<Material>(*_material); u->_path.clear(); _material = u; _baked = false; }
	}

	(void)tintChanged; // 틴트는 셰이더 gObjTint 로 적용 — 재베이크 불필요
}

// RT 통합 — AS 패스에서 호출(Draw 전). 트랜스폼 변경 시에만 월드 정점 재생성.
void MeshRenderer::UpdateWorld()
{
	if (!_dev || _local.empty()) return;
	SyncMaterialTex();
	uint32 ver = 0; if (auto t = GetTransform()) ver = t->Version();
	if (!_baked || ver != _bakedVer) { Rebake(); _baked = true; _bakedVer = ver; _blasDirty = true; }
}

void MeshRenderer::RecordBuildBLAS(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || _local.empty()) return;
	if (_blas && !_blasDirty) return; // 정적: 기존 BLAS 유지 (재빌드 생략)
	RtBlas::Build(_dev->_device.Get(), cmd, _vb.Get(), _ib.Get(),
	              (UINT)_local.size(), (UINT)_indices.size(), sizeof(Vtx), _blas, _blasScratch);
	_blasDirty = false;
}

// 선택 아웃라인 — 월드 베이크 정점을 그대로(인버티드 헐은 셰이더가 노멀 팽창) 드로우.
void MeshRenderer::RecordOutline(ID3D12GraphicsCommandList4* cmd)
{
	if (!_dev || _local.empty() || !_vb) return;
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced((UINT)_indices.size(), 1, 0, 0, 0);
}

void MeshRenderer::Draw(const RenderContext& ctx)
{
	if (!_dev || _local.empty()) return;

	UpdateWorld(); // 더티 아니면 no-op (AS 패스 미수집 오브젝트 안전망)

	D3D12Device& d = *_dev;
	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, ctx.cb); // 카메라별 CB (Scene/Game)
	cmd->SetGraphicsRootShaderResourceView(1, d._scene._tlas->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr());
	// per-object 머티리얼 루트상수 (mode, metallic, roughness, emissive, tint.rgb, pad)
	struct { uint32 mode; float met, rough, emis, tr, tg, tb, pad; } mc{
		_hasTex ? 1u : 2u, _material->_metallic, _material->_roughness, _material->_emissive,
		_material->_diffuse.x, _material->_diffuse.y, _material->_diffuse.z, 0.f };
	if (_hasTex)
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetGraphicsRootDescriptorTable(3, _srvHeap->GetGPUDescriptorHandleForHeapStart());
	}
	cmd->SetGraphicsRoot32BitConstants(4, 8, &mc, 0);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced((UINT)_indices.size(), 1, 0, 0, 0);
}
