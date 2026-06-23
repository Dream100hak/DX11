#include "GameView.h"
#include "D3D12Device.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "Renderer.h"
#include "RenderContext.h"
#include "imgui.h"

using namespace DirectX;

void GameView::Init(D3D12Device* dev, DXGI_FORMAT sceneFmt)
{
	_dev = dev;
	_sceneFmt = sceneFmt;
	_postfx.Init(dev->Device(), sceneFmt);
}

// Game 뷰 오프스크린 RT (게임 카메라 시점) — CreateSceneRT 미러
void GameView::CreateRT(UINT w, UINT h)
{
	_w = w; _h = h;
	ID3D12Device5* device = _dev->_device.Get();
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = _sceneFmt; rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cvc{}; cvc.Format = rd.Format; cvc.Color[3] = 1.0f;
	_rt.Reset();
	ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvc, IID_PPV_ARGS(&_rt)), "game RT");
	_rtState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (!_rtvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; ThrowIfFailed(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_rtvHeap)), "game RTV heap"); }
	device->CreateRenderTargetView(_rt.Get(), nullptr, _rtvHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RESOURCE_DESC dd{}; dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1; dd.Format = DXGI_FORMAT_R32_TYPELESS; dd.SampleDesc.Count = 1;
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cvd{}; cvd.Format = DXGI_FORMAT_D32_FLOAT; cvd.DepthStencil.Depth = 1.0f;
	_depth.Reset();
	ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cvd, IID_PPV_ARGS(&_depth)), "game depth");
	_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (!_dsvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; ThrowIfFailed(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_dsvHeap)), "game DSV heap"); }
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format = DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(_depth.Get(), &dsv, _dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// 게임뷰 속도 G버퍼 (RG16F) — 모션블러용
	D3D12_RESOURCE_DESC gvrd{}; gvrd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	gvrd.Width = w; gvrd.Height = h; gvrd.DepthOrArraySize = 1; gvrd.MipLevels = 1; gvrd.Format = DXGI_FORMAT_R16G16_FLOAT; gvrd.SampleDesc.Count = 1;
	gvrd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE gvcv{}; gvcv.Format = DXGI_FORMAT_R16G16_FLOAT;
	_velRT.Reset();
	ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &gvrd,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &gvcv, IID_PPV_ARGS(&_velRT)), "game vel RT");
	_velRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (!_velRtvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; ThrowIfFailed(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_velRtvHeap)), "game vel RTV heap"); }
	device->CreateRenderTargetView(_velRT.Get(), nullptr, _velRtvHeap->GetCPUDescriptorHandleForHeapStart());

	_postfx.Resize(w, h, _rt.Get(), _depth.Get(), _velRT.Get());
	_texId = _dev->_imgui.SetGameTexture(_postfx.LdrResource());
}

// 게임 카메라 시점 별도 RT 렌더 (DDGI/TLAS 재사용). Scene 패스 직후 D3D12Device::Render 가 호출.
void GameView::Render()
{
	if (!_dev->_gameScene) return;
	// 선택된 게임 카메라(비-에디터) = 씬뷰 프리뷰 인셋 대상 — Game 창이 닫혀 있어도 이때는 렌더해야 인셋 갱신
	auto& sel = _dev->_selectedGO;
	bool wantPreview = sel && sel->IsActive() && !sel->IsEditorInternal() && sel->GetCamera();
	if (!_windowOpen && !wantPreview) return;

	shared_ptr<GameObject> camObj;
	if (wantPreview) camObj = sel;
	else for (auto& kv : _dev->_gameScene->GetCreatedObjects())
		if (auto& o = kv.second; o && o->IsActive() && !o->IsEditorInternal() && o->GetCamera()) { camObj = o; break; }

	if (_pendingW && (_pendingW != _w || _pendingH != _h)) CreateRT(_pendingW, _pendingH);
	if (!_rt) CreateRT(640, 360); // Game 창 미오픈 시 기본 크기 보장(인셋 프리뷰용)
	if (!_rt) return;

	auto* cmd = _dev->_cmdList.Get();
	_dev->Transition(_rt.Get(), _rtState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_dev->Transition(_depth.Get(), _depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	float clr[4] = { 0.02f, 0.02f, 0.03f, 1.f };
	cmd->ClearRenderTargetView(rtv, clr, 0, nullptr);
	cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	D3D12_VIEWPORT vp{ 0, 0, float(_w), float(_h), 0, 1 }; D3D12_RECT scr{ 0, 0, (LONG)_w, (LONG)_h };
	cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &scr);

	// 모션블러용 — 게임 카메라 invVP(비지터드)와 직전 프레임 VP 보존(블록 안 갱신 전 캡처)
	Matrix gameInvVP{}; Matrix gamePrevForMB = _prevVP; bool gameHadPrev = _hasPrevVP; bool gameValid = false;

	if (camObj)
	{
		auto t = camObj->GetTransform(); auto cam = camObj->GetCamera();
		Matrix wm = t->GetWorldMatrix(); XMMATRIX W = XMLoadFloat4x4(&wm);
		XMMATRIX view = XMMatrixInverse(nullptr, W);
		float aspect = float(_w) / float(_h);
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(cam->_fov), aspect, cam->_near, cam->_far);

		SceneCB g = _dev->_cbCache; // 에디터 CB 베이스 + 카메라 필드 교체
		XMStoreFloat4x4(&g.mvp, view * proj);
		XMVECTOR eye = W.r[3]; XMStoreFloat4(&g.camPos, eye);
		XMStoreFloat4x4(&g.invVP, XMMatrixInverse(nullptr, view * proj));
		XMStoreFloat4x4(&g.curVPnj, view * proj);   // 속도 패스(게임 카메라)
		g.prevVPnj = _prevVP;
		gameInvVP = g.invVP; gameValid = true;      // 모션블러 camVel 복원용
		memcpy(_dev->_gameCBMapped, &g, sizeof(g));

		RenderContext ctx{}; ctx.cmd = cmd; ctx.cb = _dev->_gameCB->GetGPUVirtualAddress();
		XMStoreFloat4x4(&ctx.view, view); XMStoreFloat4x4(&ctx.proj, proj);
		cam->SetView(ctx.view); cam->SetProjection(ctx.proj); cam->SortGameObject();
		auto draw = [&](vector<shared_ptr<GameObject>>& b) { for (auto& o : b) { if (!o || !o->IsActive()) continue; auto r = o->GetRenderer(); if (r) r->Draw(ctx); } };
		draw(cam->GetVecBackground()); // 스카이
		draw(cam->GetVecOpaque());     // 모델/메시/애니 (그리드=Transparent 생략)

		// ── 속도 G버퍼 (모션블러 ON) — 게임 카메라 기준 오브젝트 고유 속도 → _velRT (깊이 재사용) ──
		if (_dev->_motionBlurOn && _postfx.Ready())
		{
			_dev->Transition(_velRT.Get(), _velRTState, D3D12_RESOURCE_STATE_RENDER_TARGET);
			D3D12_CPU_DESCRIPTOR_HANDLE vrtv = _velRtvHeap->GetCPUDescriptorHandleForHeapStart();
			cmd->OMSetRenderTargets(1, &vrtv, FALSE, &dsv);
			const float zero[4] = { 0, 0, 0, 0 };
			cmd->ClearRenderTargetView(vrtv, zero, 0, nullptr);
			cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &scr);
			cmd->SetPipelineState(_dev->_velPSO.Get());
			cmd->SetGraphicsRootSignature(_dev->_rootSig.Get());
			cmd->SetGraphicsRootConstantBufferView(0, _dev->_gameCB->GetGPUVirtualAddress());
			for (auto& o : cam->GetVecOpaque())
			{
				if (!o || !o->IsActive()) continue;
				auto r = o->GetRenderer(); if (r) r->RecordVelocity(cmd);
			}
			_dev->Transition(_velRT.Get(), _velRTState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		XMStoreFloat4x4(&_prevVP, view * proj); _hasPrevVP = true; // 다음 프레임용
	}

	// 포스트 (블룸/톤맵/FXAA — 게임 전용 PostFX)
	_dev->Transition(_rt.Get(), _rtState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	bool bloomA = _dev->_bloomOn && _postfx.Ready();
	if (bloomA) _postfx.Bloom(cmd, _dev->_bloomThreshold);
	_dev->Transition(_depth.Get(), _depthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PostFX::TonemapParams tp{};
	tp.exposure = _dev->_exposure * powf(2.f, _dev->_ev); tp.bloomIntensity = _dev->_bloomIntensity; tp.bloomEnabled = bloomA; tp.tonemapOp = _dev->_tonemapOp;
	tp.contrast = _dev->_contrast; tp.saturation = _dev->_saturation; tp.temperature = _dev->_temperature; tp.vignette = _dev->_vignette;
	tp.time = _dev->_time * 60.f; tp.expScale = _dev->_expScale;
	_postfx.Tonemap(cmd, tp);
	ID3D12Resource* disp = _postfx.Fxaa(cmd, _dev->_fxaaOn);
	// 카메라 + 오브젝트 모션블러 (게임 카메라 기준) — velRT(오브젝트) + depth 재투영(카메라) 합산
	if (_dev->_motionBlurOn && gameValid && gameHadPrev)
		disp = _postfx.MotionBlur(cmd, true, disp, gameInvVP, gamePrevForMB, _dev->_motionBlurIntensity);
	_texId = _dev->_imgui.SetGameTexture(disp);
}

// "Game" 도킹 창 — 렌더 결과 표시 (DrawSceneView 끝에서 호출)
void GameView::DrawWindow()
{
	ImGui::Begin("Game");
	_windowOpen = !ImGui::IsWindowCollapsed();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	_pendingW = (UINT)max(8.0f, avail.x);
	_pendingH = (UINT)max(8.0f, avail.y);
	if (_texId) ImGui::Image((ImTextureID)_texId, avail);
	ImGui::End();
}
