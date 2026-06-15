#include "D3D12Device.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"
#include <filesystem>

namespace fs = std::filesystem;

// wstring → UTF-8 (ImGui 텍스트용)
static std::string WToUtf8(const std::wstring& w)
{
	if (w.empty()) return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

void D3D12Device::InitEditor()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = nullptr; // imgui.ini 미저장 (런타임 아티팩트 방지)
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(_hwnd);
	_imgui.Init(_device.Get(), _queue.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, FRAME_COUNT);

	DirectX::XMStoreFloat4x4(&_viewM, DirectX::XMMatrixIdentity()); // 첫 프레임 ImGuizmo NaN 방지
	DirectX::XMStoreFloat4x4(&_projM, DirectX::XMMatrixIdentity());

	// 에셋 루트 = exe\..\Resources\Assets
	wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
	_assetRoot = fs::weakly_canonical(fs::path(dir) / L".." / L"Resources" / L"Assets").wstring();
	_curDir = _assetRoot;

	CreateSceneRT(_width, _height); // 씬 오프스크린 RT 초기 생성
	_editorReady = true;
}

// 씬 오프스크린 RT + 깊이 (재)생성 — Scene 창 리사이즈 시 호출 (전체 플러시로 GPU 유휴 보장)
void D3D12Device::CreateSceneRT(UINT w, UINT h)
{
	_sceneW = w; _sceneH = h;

	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 컬러 RT
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = _sceneFmt; rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cvc{}; cvc.Format = rd.Format;
	cvc.Color[0] = 0.06f; cvc.Color[1] = 0.07f; cvc.Color[2] = 0.10f; cvc.Color[3] = 1.0f;
	_sceneRT.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvc, IID_PPV_ARGS(&_sceneRT)), "scene RT");
	_sceneRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	if (!_sceneRtvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 2; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_sceneRtvHeap)), "scene RTV heap");
	}
	UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv0 = _sceneRtvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateRenderTargetView(_sceneRT.Get(), nullptr, rtv0); // slot0 = HDR 씬

	// LDR 해상 RT (톤맵 결과, ImGui 표시) — slot1 RTV
	D3D12_RESOURCE_DESC ld = rd; ld.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_CLEAR_VALUE cvl{}; cvl.Format = ld.Format; cvl.Color[3] = 1.0f;
	_sceneLDR.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &ld,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvl, IID_PPV_ARGS(&_sceneLDR)), "scene LDR");
	_sceneLDRState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv1 = rtv0; rtv1.ptr += rtvInc;
	_device->CreateRenderTargetView(_sceneLDR.Get(), nullptr, rtv1);

	// 블룸 RT (반해상도 ping-pong) + RTV
	_bloomW = max(1u, w / 2); _bloomH = max(1u, h / 2);
	D3D12_RESOURCE_DESC bd = rd; bd.Width = _bloomW; bd.Height = _bloomH; bd.Format = _sceneFmt;
	D3D12_CLEAR_VALUE cvb{}; cvb.Format = _sceneFmt;
	_bloomA.Reset(); _bloomB.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvb, IID_PPV_ARGS(&_bloomA)), "bloomA");
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvb, IID_PPV_ARGS(&_bloomB)), "bloomB");
	_bloomAState = _bloomBState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (!_bloomRtvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC bh{}; bh.NumDescriptors = 2; bh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&bh, IID_PPV_ARGS(&_bloomRtvHeap)), "bloom RTV heap");
	}
	D3D12_CPU_DESCRIPTOR_HANDLE brtv = _bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateRenderTargetView(_bloomA.Get(), nullptr, brtv); brtv.ptr += rtvInc;
	_device->CreateRenderTargetView(_bloomB.Get(), nullptr, brtv);

	// 포스트 힙 SRV: 0=HDR씬 / 1=bloomA(최종·톤맵 t1) / 2=bloomB / 3=bloomB(더미)
	D3D12_SHADER_RESOURCE_VIEW_DESC hsd{};
	hsd.Format = _sceneFmt; hsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	hsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; hsd.Texture2D.MipLevels = 1;
	ID3D12Resource* srvSrc[4] = { _sceneRT.Get(), _bloomA.Get(), _bloomB.Get(), _bloomB.Get() };
	D3D12_CPU_DESCRIPTOR_HANDLE ph = _postSrvHeap->GetCPUDescriptorHandleForHeapStart();
	for (int k = 0; k < 4; ++k) { _device->CreateShaderResourceView(srvSrc[k], &hsd, ph); ph.ptr += _postSrvInc; }
	_bloomReady = true;

	// 깊이
	D3D12_RESOURCE_DESC dd{};
	dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1;
	dd.Format = DXGI_FORMAT_D32_FLOAT; dd.SampleDesc.Count = 1;
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cvd{}; cvd.Format = DXGI_FORMAT_D32_FLOAT; cvd.DepthStencil.Depth = 1.0f;
	_sceneDepth.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cvd, IID_PPV_ARGS(&_sceneDepth)), "scene depth");
	if (!_sceneDsvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_sceneDsvHeap)), "scene DSV heap");
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format = DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_sceneDepth.Get(), &dsv, _sceneDsvHeap->GetCPUDescriptorHandleForHeapStart());

	_sceneTexId = _imgui.SetSceneTexture(_sceneLDR.Get()); // ImGui 는 톤맵된 LDR 표시
}

// ImGui::NewFrame ~ ImGui::Render — 도킹 + 패널 구성 (Render() 초반 CPU 단계에서 호출)
void D3D12Device::BuildUI()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	// 전체화면 도킹 호스트 (배경 없음 → 중앙 패스스루로 뒤의 3D 씬뷰 노출)
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->WorkPos);
	ImGui::SetNextWindowSize(vp->WorkSize);
	ImGui::SetNextWindowViewport(vp->ID);
	ImGuiWindowFlags host = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("DockHost", nullptr, host);
	ImGui::PopStyleVar(3);

	ImGuiID dockId = ImGui::GetID("RtDemoDock");

	// 최초 1회 기본 레이아웃 — Inspector 우측 / FolderContents 하단 / 중앙 = 씬뷰(패스스루)
	static bool built = false;
	if (!built)
	{
		built = true;
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
		ImGuiID center = dockId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.30f, nullptr, &center);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
		ImGui::DockBuilderDockWindow("Hierarchy", left);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("FolderContents", bottom);
		ImGui::DockBuilderDockWindow("Scene", center);
		ImGui::DockBuilderFinish(dockId);
	}
	ImGui::DockSpace(dockId, ImVec2(0, 0));
	ImGui::End();

	DrawSceneView();
	DrawHierarchy();
	DrawInspector();
	DrawFolderContents();

	ImGui::Render();
}

void D3D12Device::DrawHierarchy()
{
	ImGui::Begin("Hierarchy");
	ImGui::TextDisabled("Scene");
	ImGui::Separator();

	std::string modelItem = "[Mdl] " + WToUtf8(_modelLabel);
	struct Entry { std::string label; SelEntity e; };
	const Entry items[] = {
		{ "[Cam] Editor Camera", SelEntity::Camera },
		{ "[Sun] Directional Light", SelEntity::Sun },
		{ "[GI]  DDGI Volume", SelEntity::DDGI },
		{ "[Pt]  Point Light", SelEntity::Point },
		{ modelItem, SelEntity::Model },
		{ "[Geo] Floor", SelEntity::Floor },
	};
	for (const Entry& it : items)
	{
		if (ImGui::Selectable(it.label.c_str(), _sel == it.e))
			_sel = it.e;
	}
	ImGui::End();
}

void D3D12Device::DrawSceneView()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Scene");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	_pendingSceneW = (UINT)max(8.0f, avail.x);
	_pendingSceneH = (UINT)max(8.0f, avail.y);
	ImVec2 imgPos = ImGui::GetCursorScreenPos();
	if (_sceneTexId)
		ImGui::Image((ImTextureID)_sceneTexId, avail);
	_sceneHovered = ImGui::IsWindowHovered();
	_sceneFocused = ImGui::IsWindowFocused();

	// ── 클릭 픽킹 (좌클릭, 기즈모 위 아닐 때) ──
	if (_sceneHovered && ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
	{
		ImVec2 mp = ImGui::GetMousePos();
		float u = (mp.x - imgPos.x) / avail.x, v = (mp.y - imgPos.y) / avail.y;
		if (u >= 0 && u <= 1 && v >= 0 && v <= 1) PickAt(u, v);
	}

	// ── ImGuizmo: 모델/점광원 트랜스폼 조작 (이미지 영역에 오버레이) ──
	if ((_sel == SelEntity::Model && _modelMatrixInit) || _sel == SelEntity::Point)
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(imgPos.x, imgPos.y, avail.x, avail.y);
		if (_sel == SelEntity::Model)
		{
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				(ImGuizmo::OPERATION)_gizmoOp, ImGuizmo::WORLD, (float*)&_modelMatrix);
		}
		else // Point — 이동만
		{
			DirectX::XMFLOAT4X4 m; DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixTranslation(_pointPos.x, _pointPos.y, _pointPos.z));
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				ImGuizmo::TRANSLATE, ImGuizmo::WORLD, (float*)&m);
			_pointPos = { m._41, m._42, m._43 };
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

// 씬뷰 클릭 레이 → 모델 AABB / 바닥 평면 픽킹 → 하이어라키 선택
void D3D12Device::PickAt(float u, float v)
{
	using namespace DirectX;
	float nx = u * 2.0f - 1.0f, ny = (1.0f - v) * 2.0f - 1.0f;
	XMMATRIX invVP = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_viewM) * XMLoadFloat4x4(&_projM));
	XMVECTOR n = XMVector4Transform(XMVectorSet(nx, ny, 0, 1), invVP);
	XMVECTOR f = XMVector4Transform(XMVectorSet(nx, ny, 1, 1), invVP);
	n = XMVectorScale(n, 1.0f / XMVectorGetW(n));
	f = XMVectorScale(f, 1.0f / XMVectorGetW(f));
	XMFLOAT3 ro; XMStoreFloat3(&ro, n);
	XMFLOAT3 rdv; XMStoreFloat3(&rdv, XMVector3Normalize(XMVectorSubtract(f, n)));

	// 모델 AABB 슬랩 테스트
	float tModel = 1e9f; bool hitM = false;
	{
		float tmin = 0.0f, tmax = 1e9f; bool ok = true;
		const float* ros = &ro.x; const float* rds = &rdv.x;
		const float* bmn = &_modelMin.x; const float* bmx = &_modelMax.x;
		for (int a = 0; a < 3; ++a)
		{
			if (fabsf(rds[a]) < 1e-7f) { if (ros[a] < bmn[a] || ros[a] > bmx[a]) { ok = false; break; } }
			else { float inv = 1.0f / rds[a]; float t1 = (bmn[a] - ros[a]) * inv, t2 = (bmx[a] - ros[a]) * inv;
				if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
				tmin = max(tmin, t1); tmax = min(tmax, t2); if (tmin > tmax) { ok = false; break; } }
		}
		if (ok && tmax > 0) { hitM = true; tModel = max(tmin, 0.0f); }
	}
	// 바닥 평면 y=0 (±6)
	float tFloor = 1e9f; bool hitF = false;
	if (fabsf(rdv.y) > 1e-6f)
	{
		float t = -ro.y / rdv.y;
		if (t > 0) { float hx = ro.x + rdv.x * t, hz = ro.z + rdv.z * t;
			if (fabsf(hx) <= 6.0f && fabsf(hz) <= 6.0f) { hitF = true; tFloor = t; } }
	}

	if (hitM && (!hitF || tModel <= tFloor)) _sel = SelEntity::Model;
	else if (hitF) _sel = SelEntity::Floor;
}

void D3D12Device::DrawInspector()
{
	ImGui::Begin("Inspector");

	// 하이어라키 선택 대상별 프로퍼티
	switch (_sel)
	{
	case SelEntity::Camera:
		ImGui::SeparatorText("Editor Camera");
		ImGui::Text("Pos   %.2f  %.2f  %.2f", _camPos.x, _camPos.y, _camPos.z);
		ImGui::Text("Yaw %.2f   Pitch %.2f", _camYaw, _camPitch);
		ImGui::Spacing();
		ImGui::SeparatorText("Post-process");
		ImGui::SliderFloat("Exposure", &_exposure, 0.1f, 4.0f);
		ImGui::TextDisabled("ACES tonemap + gamma (HDR pipeline)");
		ImGui::Spacing();
		ImGui::TextDisabled("RMB drag = look,  WASD = move");
		ImGui::TextDisabled("Q/E = up/down,  Shift = fast");
		break;

	case SelEntity::Sun:
		ImGui::SeparatorText("Directional Light");
		ImGui::SliderFloat("Intensity", &_lightIntensity, 0.0f, 4.0f);
		ImGui::Checkbox("Animate Sun", &_lightAnimate);
		if (!_lightAnimate)
			ImGui::SliderFloat("Sun Angle", &_lightAngle, -3.14159f, 3.14159f);
		ImGui::TextDisabled("RT shadow + DDGI react in real time");
		break;

	case SelEntity::DDGI:
		ImGui::SeparatorText("DDGI Volume");
		ImGui::SliderFloat("GI Strength", &_giStrength, 0.0f, 1.5f);
		ImGui::SliderFloat("Ambient", &_ambient, 0.0f, 0.2f);
		ImGui::Spacing();
		ImGui::Text("Probes : %u  (%dx%dx%d)", PROBE_COUNT, PROBE_X, PROBE_Y, PROBE_Z);
		ImGui::Text("Oct depth: %dx%d / probe", PROBE_OCT, PROBE_OCT);
		ImGui::TextDisabled("SH-L1 irradiance + Chebyshev visibility");
		break;

	case SelEntity::Model:
		ImGui::SeparatorText(("Model: " + WToUtf8(_modelLabel)).c_str());
		// 기즈모 조작 모드 (ImGuizmo: TRANSLATE=7 / ROTATE=120 / SCALE=896)
		ImGui::TextDisabled("Gizmo:");
		ImGui::SameLine(); if (ImGui::RadioButton("Move", _gizmoOp == 7)) _gizmoOp = 7;
		ImGui::SameLine(); if (ImGui::RadioButton("Rotate", _gizmoOp == 120)) _gizmoOp = 120;
		ImGui::SameLine(); if (ImGui::RadioButton("Scale", _gizmoOp == 896)) _gizmoOp = 896;
		if (ImGui::Button("Reset Transform")) { DirectX::XMStoreFloat4x4(&_modelMatrix, DirectX::XMMatrixIdentity()); }
		ImGui::Spacing();
		ImGui::SeparatorText("Material (PBR)");
		ImGui::SliderFloat("Metallic", &_matMetallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &_matRoughness, 0.02f, 1.0f);
		ImGui::SliderFloat("Emissive", &_matEmissive, 0.0f, 5.0f);
		ImGui::SliderFloat("Albedo Tint", &_matTint, 0.0f, 2.0f);
		ImGui::Spacing();
		ImGui::Text("Vertices : %u", _vertexCount);
		ImGui::Text("Triangles: %u", _indexCount / 3);
		ImGui::Text("Bones    : %u", (unsigned)_bonesData.size());
		ImGui::Text("Submeshes: %u   Materials: %u", (unsigned)_submeshes.size(), _matCount);
		if (!_submeshes.empty() && ImGui::TreeNodeEx("Submeshes / Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (size_t i = 0; i < _submeshes.size(); ++i)
				ImGui::BulletText("%s  (%u tris, slot %u)",
					_submeshes[i].materialName.c_str(), _submeshes[i].indexCount / 3,
					i < _subMatSlot.size() ? _subMatSlot[i] : 0u);
			ImGui::TreePop();
		}
		break;

	case SelEntity::Point:
		ImGui::SeparatorText("Point Light");
		ImGui::Checkbox("Enabled", &_pointOn);
		ImGui::DragFloat3("Position", &_pointPos.x, 0.05f);
		ImGui::ColorEdit3("Color", &_pointColor.x);
		ImGui::SliderFloat("Intensity", &_pointIntensity, 0.0f, 12.0f);
		ImGui::SliderFloat("Radius", &_pointRadius, 0.5f, 20.0f);
		ImGui::TextDisabled("RT shadow ray to point; move via gizmo too");
		break;

	case SelEntity::Floor:
		ImGui::SeparatorText("Floor");
		ImGui::TextDisabled("Static quad (vertex color, red)");
		ImGui::Text("Bounces red indirect light via DDGI");
		break;
	}

	// 선택된 에셋 정보 (FolderContents 에서 클릭)
	if (!_selectedAsset.empty())
	{
		ImGui::Separator();
		ImGui::SeparatorText("Selected Asset");
		fs::path p(_selectedAsset);
		ImGui::Text("Name: %s", WToUtf8(p.filename().wstring()).c_str());
		ImGui::Text("Type: %s", WToUtf8(p.extension().wstring()).c_str());
		std::error_code ec; auto sz = fs::file_size(p, ec);
		if (!ec) ImGui::Text("Size: %.1f KB", sz / 1024.0);
	}

	ImGui::End();
}

void D3D12Device::DrawFolderContents()
{
	ImGui::Begin("FolderContents");

	// 경로 바 + 상위 폴더
	bool atRoot = (fs::path(_curDir) == fs::path(_assetRoot));
	if (!atRoot)
	{
		if (ImGui::Button("..")) _curDir = fs::path(_curDir).parent_path().wstring();
		ImGui::SameLine();
	}
	std::wstring rel = fs::relative(_curDir, fs::path(_assetRoot).parent_path()).wstring();
	ImGui::TextDisabled("%s", WToUtf8(rel).c_str());
	ImGui::Separator();

	std::error_code ec;
	std::vector<fs::path> dirs, files;
	for (auto& e : fs::directory_iterator(_curDir, ec))
	{
		if (e.is_directory(ec)) dirs.push_back(e.path());
		else                    files.push_back(e.path());
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());

	for (auto& d : dirs)
	{
		std::string label = "[/] " + WToUtf8(d.filename().wstring());
		if (ImGui::Selectable(label.c_str(), false))
			_curDir = d.wstring();
	}
	for (auto& f : files)
	{
		bool isMesh = (f.extension() == L".mesh");
		std::string label = (isMesh ? "[mesh] " : "") + WToUtf8(f.filename().wstring());
		bool sel = (f.wstring() == _selectedAsset);
		if (ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick))
		{
			_selectedAsset = f.wstring();
			// .mesh 더블클릭 → 모델 교체 (다음 프레임 GPU 유휴 시 처리)
			if (isMesh && ImGui::IsMouseDoubleClicked(0))
			{
				_pendingModel = f.wstring();
				_sel = SelEntity::Model;
			}
		}
	}
	ImGui::TextDisabled("Double-click a [mesh] to load");

	ImGui::End();
}
