#include "D3D12Device.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

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

// 디버그 라인 빌드 + 드로우 (U10 본 / U11 AABB / U12 라이트 아이콘 / U13 스팟 콘)
void D3D12Device::DrawDebugLines()
{
	using namespace DirectX;
	std::vector<float> v;
	auto line = [&](XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c) {
		float arr[12] = { a.x,a.y,a.z, c.x,c.y,c.z, b.x,b.y,b.z, c.x,c.y,c.z };
		v.insert(v.end(), arr, arr + 12);
	};
	auto cross = [&](XMFLOAT3 p, XMFLOAT3 c, float s) {
		line({ p.x - s,p.y,p.z }, { p.x + s,p.y,p.z }, c); line({ p.x,p.y - s,p.z }, { p.x,p.y + s,p.z }, c); line({ p.x,p.y,p.z - s }, { p.x,p.y,p.z + s }, c);
	};

	if (_showAABB && _sel == SelEntity::Model)
	{
		XMFLOAT3 n = _modelMin, x = _modelMax, c{ 1.0f, 0.6f, 0.1f };
		XMFLOAT3 p[8] = { {n.x,n.y,n.z},{x.x,n.y,n.z},{x.x,n.y,x.z},{n.x,n.y,x.z},{n.x,x.y,n.z},{x.x,x.y,n.z},{x.x,x.y,x.z},{n.x,x.y,x.z} };
		int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
		for (int k = 0; k < 24; k += 2) line(p[e[k]], p[e[k + 1]], c);
	}
	if (_showBones)
		for (size_t b = 0; b < _boneWorld.size(); ++b)
		{ int par = _bonesData[b].parent; if (par >= 0 && par < (int)_boneWorld.size()) line(_boneWorld[b], _boneWorld[par], { 0.2f, 1.0f, 1.0f }); }
	if (_showLightIcons)
	{
		if (_pointOn) cross(_pointPos, _pointColor, 0.18f);
		for (int li = 1; li < _ptCount; ++li) cross({ _ptPosArr[li].x,_ptPosArr[li].y,_ptPosArr[li].z }, { _ptColArr[li].x,_ptColArr[li].y,_ptColArr[li].z }, 0.18f);
		if (_spotOn) cross(_spotPos, _spotColor, 0.18f);
	}
	if (_spotOn && _showSpotCone)
	{
		XMVECTOR ap = XMLoadFloat3(&_spotPos);
		XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&_spotDir));
		XMVECTOR up = fabsf(XMVectorGetY(dir)) > 0.95f ? XMVectorSet(1,0,0,0) : XMVectorSet(0,1,0,0);
		XMVECTOR rt = XMVector3Normalize(XMVector3Cross(up, dir)); XMVECTOR bt = XMVector3Cross(dir, rt);
		float len = min(_spotRadius, 5.0f), rad = tanf(_spotConeDeg * 0.01745f) * len;
		XMVECTOR ce = XMVectorAdd(ap, XMVectorScale(dir, len));
		XMFLOAT3 apex; XMStoreFloat3(&apex, ap); XMFLOAT3 col{ _spotColor.x, _spotColor.y, _spotColor.z };
		XMFLOAT3 prev{};
		for (int s = 0; s <= 16; ++s)
		{
			float a = s / 16.0f * 6.2831853f;
			XMVECTOR pw = XMVectorAdd(ce, XMVectorAdd(XMVectorScale(rt, cosf(a) * rad), XMVectorScale(bt, sinf(a) * rad)));
			XMFLOAT3 p; XMStoreFloat3(&p, pw);
			if (s > 0) line(prev, p, col);
			if (s % 4 == 0) line(apex, p, col);
			prev = p;
		}
	}

	if (v.empty()) return;
	UINT bytes = (UINT)(v.size() * sizeof(float));
	if (_dbgCap < bytes)
	{
		_dbgCap = bytes + 8192; _dbgVB = CreateUploadBuffer(nullptr, _dbgCap);
		D3D12_RANGE nr{ 0, 0 }; _dbgVB->Map(0, &nr, &_dbgMapped);
	}
	memcpy(_dbgMapped, v.data(), bytes);
	D3D12_VERTEX_BUFFER_VIEW vbv{ _dbgVB->GetGPUVirtualAddress(), bytes, 24 };
	_cmdList->SetPipelineState(_dbgPSO.Get());
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	_cmdList->IASetVertexBuffers(0, 1, &vbv);
	_cmdList->DrawInstanced((UINT)(v.size() / 6), 1, 0, 0);
}

void D3D12Device::Log(const std::string& m) { _log.push_back(m); if (_log.size() > 200) _log.erase(_log.begin()); }

void D3D12Device::PushUndo()
{
	Snapshot s; s.m = _modelMatrix; s.met = _matMetallic; s.rough = _matRoughness; s.emis = _matEmissive; s.tint = _matTint; s.dt = _diffuseTint;
	_undo.push_back(s); if (_undo.size() > 64) _undo.erase(_undo.begin()); _redo.clear();
}
void D3D12Device::DoUndo()
{
	if (_undo.empty()) return;
	Snapshot cur; cur.m = _modelMatrix; cur.met = _matMetallic; cur.rough = _matRoughness; cur.emis = _matEmissive; cur.tint = _matTint; cur.dt = _diffuseTint;
	_redo.push_back(cur);
	Snapshot s = _undo.back(); _undo.pop_back();
	_modelMatrix = s.m; _matMetallic = s.met; _matRoughness = s.rough; _matEmissive = s.emis; _matTint = s.tint; _diffuseTint = s.dt;
	Log("Undo");
}
void D3D12Device::DoRedo()
{
	if (_redo.empty()) return;
	Snapshot cur; cur.m = _modelMatrix; cur.met = _matMetallic; cur.rough = _matRoughness; cur.emis = _matEmissive; cur.tint = _matTint; cur.dt = _diffuseTint;
	_undo.push_back(cur);
	Snapshot s = _redo.back(); _redo.pop_back();
	_modelMatrix = s.m; _matMetallic = s.met; _matRoughness = s.rough; _matEmissive = s.emis; _matTint = s.tint; _diffuseTint = s.dt;
	Log("Redo");
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
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 3; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
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

	// FXAA 결과 RT (slot2 RTV) + LDR/LDR2 SRV (포스트 힙 slot4,5)
	_sceneLDR2.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &ld,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvl, IID_PPV_ARGS(&_sceneLDR2)), "scene LDR2");
	_sceneLDR2State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv2 = rtv0; rtv2.ptr += rtvInc * 2;
	_device->CreateRenderTargetView(_sceneLDR2.Get(), nullptr, rtv2);
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC lsd{}; lsd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		lsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; lsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; lsd.Texture2D.MipLevels = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE lh = _postSrvHeap->GetCPUDescriptorHandleForHeapStart(); lh.ptr += (SIZE_T)4 * _postSrvInc;
		_device->CreateShaderResourceView(_sceneLDR.Get(), &lsd, lh);  lh.ptr += _postSrvInc;
		_device->CreateShaderResourceView(_sceneLDR2.Get(), &lsd, lh); // slot5
	}

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

	// 깊이 (DOF 가 SRV 로 샘플 → 먼저 생성)
	D3D12_RESOURCE_DESC dd{};
	dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1;
	dd.Format = DXGI_FORMAT_R32_TYPELESS; dd.SampleDesc.Count = 1; // SRV(R32_FLOAT)+DSV(D32) 공용
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cvd{}; cvd.Format = DXGI_FORMAT_D32_FLOAT; cvd.DepthStencil.Depth = 1.0f;
	_sceneDepth.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cvd, IID_PPV_ARGS(&_sceneDepth)), "scene depth");
	_sceneDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (!_sceneDsvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_sceneDsvHeap)), "scene DSV heap");
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format = DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_sceneDepth.Get(), &dsv, _sceneDsvHeap->GetCPUDescriptorHandleForHeapStart());

	// 포스트 힙 SRV: 0=HDR씬 / 1=bloomA(최종·톤맵 t1) / 2=depth(톤맵 t2 DOF) / 3=bloomB
	D3D12_SHADER_RESOURCE_VIEW_DESC hsd{};
	hsd.Format = _sceneFmt; hsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	hsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; hsd.Texture2D.MipLevels = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE ph = _postSrvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateShaderResourceView(_sceneRT.Get(), &hsd, ph); ph.ptr += _postSrvInc; // 0 HDR
	_device->CreateShaderResourceView(_bloomA.Get(), &hsd, ph); ph.ptr += _postSrvInc; // 1 bloomA
	D3D12_SHADER_RESOURCE_VIEW_DESC dsd{}; dsd.Format = DXGI_FORMAT_R32_FLOAT; // 2 depth
	dsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; dsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; dsd.Texture2D.MipLevels = 1;
	_device->CreateShaderResourceView(_sceneDepth.Get(), &dsd, ph); ph.ptr += _postSrvInc;
	_device->CreateShaderResourceView(_bloomB.Get(), &hsd, ph); // 3 bloomB
	_bloomReady = true;

	_sceneTexId = _imgui.SetSceneTexture(_sceneLDR.Get()); // ImGui 는 톤맵된 LDR 표시
}

// ImGui::NewFrame ~ ImGui::Render — 도킹 + 패널 구성 (Render() 초반 CPU 단계에서 호출)
void D3D12Device::BuildUI()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	// ── 메인 메뉴바 (File / View / 우측 통계) ──
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene")) SaveScene();
			if (ImGui::MenuItem("Load Scene")) LoadScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Screenshot (PNG)")) _wantShot = true;
			if (ImGui::MenuItem("Screenshot Hi-Res 2x")) { _renderScale = 2.0f; _wantShot = true; _hiresShot = true; } // V20
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z")) DoUndo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y")) DoRedo();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", nullptr, &_showGrid);
			ImGui::MenuItem("Sky", nullptr, &_showSky);
			ImGui::MenuItem("Bloom", nullptr, &_bloomOn);
			ImGui::MenuItem("Wireframe", nullptr, &_wireframe);
			ImGui::EndMenu();
		}
		float fps = ImGui::GetIO().Framerate;
		char stat[128];
		snprintf(stat, sizeof(stat), "%.1f FPS  |  %u tris  |  %u probes  |  DXR RT", fps, _indexCount / 3, PROBE_COUNT);
		float tw = ImGui::CalcTextSize(stat).x;
		ImGui::SameLine(ImGui::GetWindowWidth() - tw - 16.0f);
		ImGui::TextDisabled("%s", stat);
		ImGui::EndMainMenuBar();
	}

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
		ImGui::DockBuilderDockWindow("Log", bottom);
		ImGui::DockBuilderDockWindow("Scene", center);
		ImGui::DockBuilderFinish(dockId);
	}
	ImGui::DockSpace(dockId, ImVec2(0, 0));
	ImGui::End();

	DrawSceneView();
	DrawHierarchy();
	DrawInspector();
	DrawFolderContents();
	DrawLog();

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
		{ "[Spt] Spot Light", SelEntity::Spot },
		{ "[FX]  Post / Render", SelEntity::Post },
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
		ImGuizmo::SetGizmoSizeClipSpace(_gizmoSize); // V6
		ImGuizmo::SetRect(imgPos.x, imgPos.y, avail.x, avail.y);
		if (_sel == SelEntity::Model)
		{
			float snapVal = (_gizmoOp == 7) ? _snapT : (_gizmoOp == 120) ? _snapR : _snapS;
			float snap3[3] = { snapVal, snapVal, snapVal };
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				(ImGuizmo::OPERATION)_gizmoOp, _gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
				(float*)&_modelMatrix, nullptr, _snapOn ? snap3 : nullptr);
		}
		else // Point — 이동만
		{
			DirectX::XMFLOAT4X4 m; DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixTranslation(_pointPos.x, _pointPos.y, _pointPos.z));
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				ImGuizmo::TRANSLATE, ImGuizmo::WORLD, (float*)&m);
			_pointPos = { m._41, m._42, m._43 };
		}
		static bool wasUsing = false;
		bool using_ = ImGuizmo::IsUsing();
		if (using_ && !wasUsing && _sel == SelEntity::Model) PushUndo(); // 조작 시작 시 되돌리기 지점
		wasUsing = using_;
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void D3D12Device::DrawLog()
{
	ImGui::Begin("Log");
	if (ImGui::Button("Clear")) _log.clear();
	ImGui::Separator();
	ImGui::BeginChild("logscroll");
	for (auto& m : _log) ImGui::TextUnformatted(m.c_str());
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::End();
}

// ── 스크린샷 (T19) — _sceneLDR 리드백 → BMP (exe 폴더) ──
void D3D12Device::SaveScreenshot()
{
	if (!_sceneLDR) return;
	D3D12_RESOURCE_DESC td = _sceneLDR->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
	_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);

	D3D12_HEAP_PROPERTIES rb{}; rb.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
	bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> readback;
	if (FAILED(_device->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return;

	ComPtr<ID3D12CommandAllocator> al; ComPtr<ID3D12GraphicsCommandList> cl;
	_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al.Get(), nullptr, IID_PPV_ARGS(&cl));
	D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = _sceneLDR.Get();
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; cl->ResourceBarrier(1, &b);
	D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
	D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = _sceneLDR.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
	cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	std::swap(b.Transition.StateBefore, b.Transition.StateAfter); cl->ResourceBarrier(1, &b);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() }; _queue->ExecuteCommandLists(1, lists);
	WaitForGpu();

	uint8_t* data = nullptr; D3D12_RANGE rr{ 0, (SIZE_T)total };
	if (FAILED(readback->Map(0, &rr, (void**)&data))) return;
	UINT W = (UINT)td.Width, H = td.Height;
	std::vector<uint8_t> rgba((size_t)W * H * 4);
	for (UINT y = 0; y < H; ++y)
		memcpy(rgba.data() + (size_t)y * W * 4, data + fp.Offset + (size_t)y * fp.Footprint.RowPitch, (size_t)W * 4);
	readback->Unmap(0, nullptr);

	// PNG 작성 (WIC)
	static int idx = 0;
	wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
	std::wstring path = dir + L"\\screenshot_" + std::to_wstring(idx++) + L".png";

	ComPtr<IWICImagingFactory> wic;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return;
	ComPtr<IWICStream> stream; wic->CreateStream(&stream);
	if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) return;
	ComPtr<IWICBitmapEncoder> enc; wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
	enc->Initialize(stream.Get(), WICBitmapEncoderNoCache);
	ComPtr<IWICBitmapFrameEncode> frame; ComPtr<IPropertyBag2> props;
	enc->CreateNewFrame(&frame, &props); frame->Initialize(props.Get());
	frame->SetSize(W, H);
	WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA; frame->SetPixelFormat(&fmt);
	frame->WritePixels(H, W * 4, (UINT)rgba.size(), rgba.data());
	frame->Commit(); enc->Commit();
	Log("Screenshot saved: " + WToUtf8(fs::path(path).filename().wstring()));
}

// ── 씬 저장/로드 (.rtscene 텍스트) — <Assets>/Scenes/quick.rtscene ──
static std::wstring QuickScenePath(const std::wstring& root)
{
	fs::path dir = fs::path(root) / L"Scenes";
	std::error_code ec; fs::create_directories(dir, ec);
	return (dir / L"quick.rtscene").wstring();
}

void D3D12Device::SaveScene()
{
	std::ofstream f(QuickScenePath(_assetRoot));
	if (!f) return;
	f << "cam " << _camPos.x << ' ' << _camPos.y << ' ' << _camPos.z << ' ' << _camYaw << ' ' << _camPitch << '\n';
	f << "sun " << _lightIntensity << ' ' << _lightAngle << ' ' << (_lightAnimate ? 1 : 0) << '\n';
	f << "point " << (_pointOn ? 1 : 0) << ' ' << _pointPos.x << ' ' << _pointPos.y << ' ' << _pointPos.z
	  << ' ' << _pointColor.x << ' ' << _pointColor.y << ' ' << _pointColor.z << ' ' << _pointIntensity << ' ' << _pointRadius << '\n';
	f << "gi " << _giStrength << ' ' << _ambient << ' ' << _exposure << '\n';
	f << "mat " << _matMetallic << ' ' << _matRoughness << ' ' << _matEmissive << ' ' << _matTint << '\n';
	f << "model " << WToUtf8((_modelDir + _modelStem + L".mesh")) << '\n';
	f << "xform";
	const float* m = &_modelMatrix._11;
	for (int i = 0; i < 16; ++i) f << ' ' << m[i];
	f << '\n';
}

void D3D12Device::LoadScene()
{
	std::ifstream f(QuickScenePath(_assetRoot));
	if (!f) return;
	std::string line;
	std::string modelUtf8;
	while (std::getline(f, line))
	{
		std::istringstream s(line); std::string tag; s >> tag;
		if (tag == "cam") s >> _camPos.x >> _camPos.y >> _camPos.z >> _camYaw >> _camPitch;
		else if (tag == "sun") { int an; s >> _lightIntensity >> _lightAngle >> an; _lightAnimate = an != 0; }
		else if (tag == "point") { int on; s >> on >> _pointPos.x >> _pointPos.y >> _pointPos.z >> _pointColor.x >> _pointColor.y >> _pointColor.z >> _pointIntensity >> _pointRadius; _pointOn = on != 0; }
		else if (tag == "gi") s >> _giStrength >> _ambient >> _exposure;
		else if (tag == "mat") s >> _matMetallic >> _matRoughness >> _matEmissive >> _matTint;
		else if (tag == "model") { std::getline(s >> std::ws, modelUtf8); }
		else if (tag == "xform") { float* m = &_pendingMatrix._11; for (int i = 0; i < 16; ++i) s >> m[i]; _hasPendingMatrix = true; }
	}
	if (!modelUtf8.empty())
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, modelUtf8.c_str(), (int)modelUtf8.size(), nullptr, 0);
		std::wstring wp(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, modelUtf8.c_str(), (int)modelUtf8.size(), wp.data(), n);
		_pendingModel = wp; // 다음 프레임 GPU 유휴 시 로드 + _pendingMatrix 적용
	}
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
		ImGui::SliderFloat("FOV", &_fov, 25.0f, 100.0f);                 // T1
		ImGui::SliderFloat("Move Speed", &_moveSpeed, 0.5f, 20.0f);      // T1
		if (ImGui::Button("Reset Camera"))                              // T1
		{ _camPos = { 3.4f, 2.4f, -4.6f }; _camYaw = -0.637f; _camPitch = -0.232f; _fov = 55.0f; }
		ImGui::SliderFloat("Near", &_camNear, 0.01f, 2.0f);             // V7
		ImGui::SliderFloat("Far", &_camFar, 20.0f, 500.0f);
		ImGui::Checkbox("Auto Orbit", &_orbit);
		ImGui::SliderFloat("EV", &_ev, -4.0f, 4.0f);                    // V13
		ImGui::SliderFloat("Gizmo Size", &_gizmoSize, 0.05f, 0.25f);    // V6
		ImGui::SeparatorText("Bookmarks");                              // V8
		for (int k = 0; k < 4; ++k)
		{
			ImGui::PushID(k);
			if (ImGui::Button("Set")) { _bm[k] = { _camPos, _camYaw, _camPitch, true }; }
			ImGui::SameLine(); if (ImGui::Button("Go") && _bm[k].set) { _camPos = _bm[k].pos; _camYaw = _bm[k].yaw; _camPitch = _bm[k].pitch; }
			ImGui::SameLine(); ImGui::Text("Cam %d %s", k + 1, _bm[k].set ? "*" : "");
			ImGui::PopID();
		}
		ImGui::TextDisabled("RMB fly: WASD/QE/Shift, W/E/R gizmo, F focus");
		break;

	case SelEntity::Sun:
		ImGui::SeparatorText("Directional Light");                      // T5
		ImGui::SliderFloat("Intensity", &_lightIntensity, 0.0f, 4.0f);
		ImGui::ColorEdit3("Sun Color", &_sunColor.x);
		ImGui::SliderFloat("Env Intensity", &_envIntensity, 0.0f, 3.0f);
		ImGui::Checkbox("Animate Sun", &_lightAnimate);
		if (!_lightAnimate) ImGui::SliderFloat("Sun Angle", &_lightAngle, -3.14159f, 3.14159f);
		ImGui::SliderFloat("Shadow Softness", &_shadowSoft, 0.0f, 0.12f); // T11
		ImGui::Checkbox("Time of Day", &_todOn);                          // V4
		if (_todOn) ImGui::SliderFloat("Hour", &_timeOfDay, 0.0f, 1.0f);
		ImGui::SeparatorText("Sky");                                    // T20
		ImGui::ColorEdit3("Zenith", &_skyZenith.x);
		ImGui::ColorEdit3("Horizon", &_skyHorizon.x);
		ImGui::SliderFloat("Sun Size", &_sunSize, 50.0f, 4000.0f);
		ImGui::TextDisabled("Presets:");                               // U8
		ImGui::SameLine(); if (ImGui::Button("Day"))    { _skyZenith = {0.13f,0.22f,0.44f}; _skyHorizon = {0.52f,0.60f,0.72f}; _sunColor = {1,0.96f,0.88f}; _lightIntensity = 1.6f; _lightAngle = 0.6f; }
		ImGui::SameLine(); if (ImGui::Button("Sunset")) { _skyZenith = {0.22f,0.15f,0.32f}; _skyHorizon = {0.95f,0.5f,0.22f}; _sunColor = {1.0f,0.55f,0.28f}; _lightIntensity = 1.3f; _lightAngle = 1.45f; _lightAnimate = false; }
		ImGui::SameLine(); if (ImGui::Button("Night"))  { _skyZenith = {0.02f,0.03f,0.09f}; _skyHorizon = {0.06f,0.08f,0.14f}; _sunColor = {0.4f,0.5f,0.7f}; _lightIntensity = 0.25f; }
		break;

	case SelEntity::DDGI:
		ImGui::SeparatorText("DDGI Volume");
		ImGui::SliderFloat("GI Strength", &_giStrength, 0.0f, 1.5f);
		ImGui::SliderFloat("Ambient", &_ambient, 0.0f, 0.2f);
		ImGui::Checkbox("Show Probes", &_probeViz);                      // T15
		ImGui::Spacing();
		ImGui::Text("Probes : %u  (%dx%dx%d)", PROBE_COUNT, PROBE_X, PROBE_Y, PROBE_Z);
		ImGui::TextDisabled("SH-L1 + Chebyshev oct-depth visibility");
		break;

	case SelEntity::Point:
	{
		ImGui::SeparatorText("Point Lights");                           // T14
		ImGui::Checkbox("Enabled (Light 0)", &_pointOn);
		ImGui::SliderInt("Light Count", &_ptCount, 1, MAX_PT);
		ImGui::Separator();
		ImGui::TextDisabled("Light 0 (gizmo-movable)");
		ImGui::DragFloat3("Position##0", &_pointPos.x, 0.05f);
		ImGui::ColorEdit3("Color##0", &_pointColor.x);
		ImGui::SliderFloat("Intensity##0", &_pointIntensity, 0.0f, 12.0f);
		ImGui::SliderFloat("Radius##0", &_pointRadius, 0.5f, 20.0f);
		ImGui::Checkbox("Orbit##0", &_ptOrbit); ImGui::SameLine(); ImGui::SliderFloat("Orbit Spd", &_ptOrbitSpeed, 0.1f, 3.0f); // V14
		for (int li = 1; li < _ptCount; ++li)
		{
			ImGui::PushID(li);
			ImGui::Separator(); ImGui::TextDisabled("Light %d", li);
			if (_ptPosArr[li].w < 0.01f) { _ptPosArr[li] = { (float)li * 1.5f, 2.0f, -1.0f, 7.0f }; _ptColArr[li] = { 2.0f, 1.0f, 0.6f, 1.0f }; }
			ImGui::DragFloat3("Position", &_ptPosArr[li].x, 0.05f);
			ImGui::SliderFloat("Radius", &_ptPosArr[li].w, 0.5f, 20.0f);
			ImGui::ColorEdit3("Color x Int", &_ptColArr[li].x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			_ptColArr[li].w = 1.0f;
			ImGui::PopID();
		}
		break;
	}

	case SelEntity::Spot:                                               // T13
		ImGui::SeparatorText("Spot Light");
		ImGui::Checkbox("Enabled", &_spotOn);
		ImGui::DragFloat3("Position", &_spotPos.x, 0.05f);
		ImGui::DragFloat3("Direction", &_spotDir.x, 0.02f);
		ImGui::ColorEdit3("Color", &_spotColor.x);
		ImGui::SliderFloat("Intensity", &_spotIntensity, 0.0f, 16.0f);
		ImGui::SliderFloat("Radius", &_spotRadius, 0.5f, 25.0f);
		ImGui::SliderFloat("Cone (deg)", &_spotConeDeg, 5.0f, 60.0f);
		break;

	case SelEntity::Post:                                               // T6/T7/T8/T9/T10/T12/T16
		ImGui::SeparatorText("Tonemap / Exposure");
		ImGui::Combo("Operator", &_tonemapOp, "ACES\0Reinhard\0Filmic\0");
		ImGui::SliderFloat("Exposure", &_exposure, 0.1f, 4.0f);
		ImGui::Checkbox("Auto Exposure", &_autoExp);
		if (_autoExp) { ImGui::SliderFloat("Target", &_expTarget, 0.1f, 1.5f); ImGui::SameLine(); ImGui::TextDisabled("(x%.2f)", _expScale); }
		ImGui::SeparatorText("Depth of Field / God Rays");
		ImGui::Checkbox("DOF", &_dofOn);
		ImGui::SliderFloat("Focus Dist", &_dofFocus, 1.0f, 30.0f);
		ImGui::SliderFloat("Focus Range", &_dofRange, 0.5f, 15.0f);
		ImGui::Checkbox("Volumetric Rays", &_volOn);
		ImGui::SliderFloat("Ray Strength", &_volStrength, 0.0f, 2.0f);
		ImGui::SeparatorText("Bloom");
		ImGui::Checkbox("Bloom", &_bloomOn);
		ImGui::SliderFloat("Threshold", &_bloomThreshold, 0.2f, 3.0f);
		ImGui::SliderFloat("Bloom Intensity", &_bloomIntensity, 0.0f, 2.0f);
		ImGui::SeparatorText("Color Grading");
		ImGui::SliderFloat("Contrast", &_contrast, 0.5f, 2.0f);
		ImGui::SliderFloat("Saturation", &_saturation, 0.0f, 2.0f);
		ImGui::SliderFloat("Temperature", &_temperature, -1.0f, 1.0f);
		ImGui::SliderFloat("Vignette", &_vignette, 0.0f, 1.0f);
		ImGui::SeparatorText("Fog / AA / Reflection");
		ImGui::ColorEdit3("Fog Color", &_fogColor.x);
		ImGui::SliderFloat("Fog Density", &_fogDensity, 0.0f, 0.08f);
		ImGui::Checkbox("FXAA", &_fxaaOn);
		ImGui::Checkbox("RT Reflection", &_reflectOn);
		ImGui::SliderFloat("Reflect Strength", &_reflectStrength, 0.0f, 1.0f);
		ImGui::SeparatorText("Ambient Occlusion (RT)");
		ImGui::Checkbox("RT AO", &_aoOn);
		ImGui::SliderFloat("AO Intensity", &_aoIntensity, 0.0f, 2.0f);
		ImGui::SliderFloat("AO Radius", &_aoRadius, 0.1f, 2.0f);
		ImGui::SeparatorText("Lens FX");
		ImGui::SliderFloat("Chromatic Aberr.", &_chroma, 0.0f, 0.02f);
		ImGui::SliderFloat("Film Grain", &_grain, 0.0f, 0.2f);
		ImGui::SliderFloat("Sharpen", &_sharpen, 0.0f, 1.5f);
		ImGui::SliderFloat("Lens Distort", &_lensDistort, -0.5f, 0.5f);   // V10
		ImGui::SliderFloat("Posterize", &_posterize, 0.0f, 16.0f);       // V11 (0=off)
		ImGui::Combo("Filter", &_filterMode, "None\0Sepia\0Grayscale\0Invert\0"); // V12
		ImGui::Checkbox("Anamorphic Bloom", &_anamorphic);               // V19
		ImGui::SliderFloat("Render Scale", &_renderScale, 0.5f, 2.0f);
		ImGui::SeparatorText("Grid / Background");
		ImGui::SliderFloat("Grid Cell", &_gridCell, 0.25f, 5.0f);        // V15
		ImGui::SliderFloat("Grid Fade", &_gridFade, 10.0f, 150.0f);
		ImGui::Combo("Background", &_bgMode, "Sky\0Solid Color\0");      // V17
		if (_bgMode == 1) ImGui::ColorEdit3("BG Color", &_bgColor.x);
		if (ImGui::Button("Reset All To Defaults")) {                    // V18
			_toonLevels = 0; _rimPower = 0; _normalIntensity = 1; _chroma = _grain = _sharpen = 0;
			_lensDistort = 0; _posterize = 0; _filterMode = 0; _anamorphic = false; _renderScale = 1;
			_contrast = _saturation = 1; _temperature = 0; _vignette = 0.25f; _ev = 0; _fogDensity = 0;
			_aoOn = _dofOn = _volOn = _autoExp = _checker = _terrain = _todOn = false; _wantReload = true;
			_bgMode = 0; _tonemapOp = 0; _exposure = 1;
		}
		ImGui::SeparatorText("Debug View / Gizmos");
		ImGui::Combo("View", &_debugView, "Lit\0Albedo\0Normal\0Depth\0GI\0");
		ImGui::Checkbox("Wireframe", &_wireframe);
		ImGui::Checkbox("Show Bones", &_showBones); ImGui::SameLine(); ImGui::Checkbox("AABB", &_showAABB);
		ImGui::Checkbox("Light Icons", &_showLightIcons); ImGui::SameLine(); ImGui::Checkbox("Spot Cone", &_showSpotCone);
		ImGui::SeparatorText("Frame Time");
		ImGui::PlotLines("ms", _frameTimes, 120, _frameIdx % 120, nullptr, 0.0f, 40.0f, ImVec2(0, 60));
		ImGui::Text("%.2f ms  /  %.0f FPS", _frameTimes[(_frameIdx + 119) % 120], ImGui::GetIO().Framerate);
		break;

	case SelEntity::Model:
	{
		ImGui::SeparatorText(("Model: " + WToUtf8(_modelLabel)).c_str());
		ImGui::TextDisabled("Gizmo:");                                  // T2
		ImGui::SameLine(); if (ImGui::RadioButton("Move", _gizmoOp == 7)) _gizmoOp = 7;
		ImGui::SameLine(); if (ImGui::RadioButton("Rotate", _gizmoOp == 120)) _gizmoOp = 120;
		ImGui::SameLine(); if (ImGui::RadioButton("Scale", _gizmoOp == 896)) _gizmoOp = 896;
		ImGui::Checkbox("Local", &_gizmoLocal); ImGui::SameLine(); ImGui::Checkbox("Snap", &_snapOn);
		if (ImGui::Button("Reset Transform")) { DirectX::XMStoreFloat4x4(&_modelMatrix, DirectX::XMMatrixIdentity()); }
		// 트랜스폼 수치 입력 (T3)
		{
			float t[3], r[3], sc[3];
			ImGuizmo::DecomposeMatrixToComponents((float*)&_modelMatrix, t, r, sc);
			bool ch = false;
			ch |= ImGui::DragFloat3("Position", t, 0.02f);
			ch |= ImGui::DragFloat3("Rotation", r, 0.5f);
			ch |= ImGui::DragFloat3("Scale", sc, 0.01f);
			if (ch) ImGuizmo::RecomposeMatrixFromComponents(t, r, sc, (float*)&_modelMatrix);
		}
		ImGui::SeparatorText("Material (PBR)");                         // T4/T5
		if (ImGui::Button("Default")) { _matMetallic = 0; _matRoughness = 0.5f; _matEmissive = 0; _matTint = 1; _diffuseTint = { 1,1,1 }; }
		ImGui::SameLine(); if (ImGui::Button("Gold")) { _matMetallic = 1; _matRoughness = 0.25f; _diffuseTint = { 1.0f, 0.78f, 0.34f }; }
		ImGui::SameLine(); if (ImGui::Button("Chrome")) { _matMetallic = 1; _matRoughness = 0.08f; _diffuseTint = { 1,1,1 }; }
		ImGui::SameLine(); if (ImGui::Button("Plastic")) { _matMetallic = 0; _matRoughness = 0.35f; _diffuseTint = { 1,1,1 }; }
		ImGui::ColorEdit3("Diffuse Tint", &_diffuseTint.x);
		ImGui::SliderFloat("Metallic", &_matMetallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &_matRoughness, 0.02f, 1.0f);
		ImGui::SliderFloat("Emissive", &_matEmissive, 0.0f, 5.0f);
		ImGui::SliderFloat("Brightness", &_matTint, 0.0f, 2.0f);
		ImGui::SliderFloat("Normal Map", &_normalIntensity, 0.0f, 3.0f); // V9
		ImGui::SeparatorText("Shading / Outline");
		ImGui::SliderInt("Toon Levels", &_toonLevels, 0, 6);             // V2 (0=off)
		ImGui::SliderFloat("Rim Power", &_rimPower, 0.0f, 8.0f);         // V3
		ImGui::ColorEdit3("Rim Color", &_rimColor.x);
		ImGui::ColorEdit3("Outline Color", &_outlineColor.x);           // V5
		ImGui::SliderFloat("Outline Width", &_outlineThick, 0.0f, 0.03f);
		// 애니메이션 (T17/T18)
		if (!_clips.empty())
		{
			ImGui::SeparatorText("Animation");
			ImGui::Checkbox("Pause", &_animPaused);
			ImGui::SliderFloat("Speed", &_animSpeed, 0.0f, 3.0f);
			std::string clipNames; for (auto& c : _clips) { clipNames += WToUtf8(fs::path(c).stem().wstring()); clipNames.push_back('\0'); }
			if (ImGui::Combo("Clip", &_curClip, clipNames.c_str()))
			{ LoadClip(_clips[_curClip], _clip); _animated = _clip.frameCount > 0; _animTimeAcc = 0.0f; }
		}
		ImGui::SeparatorText("Turntable / History");                   // U14 / U17
		ImGui::Checkbox("Auto-rotate", &_turntable); ImGui::SameLine(); ImGui::SliderFloat("Rot Speed", &_turnSpeed, 0.05f, 2.0f);
		if (ImGui::Button("Checkpoint")) PushUndo();
		ImGui::SameLine(); if (ImGui::Button("Undo")) DoUndo();
		ImGui::SameLine(); if (ImGui::Button("Redo")) DoRedo();
		ImGui::Checkbox("Show Bones", &_showBones); ImGui::SameLine(); ImGui::Checkbox("Show AABB", &_showAABB);
		ImGui::SeparatorText("Info");
		ImGui::Text("Verts %u  Tris %u  Bones %u", _vertexCount, _indexCount / 3, (unsigned)_bonesData.size());
		ImGui::Text("Submeshes %u  Materials %u", (unsigned)_submeshes.size(), _matCount);
		break;
	}

	case SelEntity::Floor:
		ImGui::SeparatorText("Floor / Ground");                        // U9 / U19
		ImGui::ColorEdit3("Color", &_floorColor.x);
		ImGui::SliderFloat("Metallic", &_floorMetallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &_floorRough, 0.02f, 1.0f);
		ImGui::Checkbox("Checker Pattern", &_checker);                  // V16
		if (ImGui::Checkbox("Terrain (heightmap)", &_terrain)) _wantReload = true; // V1
		if (ImGui::SliderFloat("Ground Size", &_groundSize, 3.0f, 20.0f)) _wantReload = true; // V19
		ImGui::TextDisabled("Bounces indirect light via DDGI");
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
