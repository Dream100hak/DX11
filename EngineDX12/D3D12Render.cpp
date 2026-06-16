#include "D3D12Device.h"
#include "Renderer.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "ParticleSystem.h"
#include "RtBlas.h"
#include "Camera.h"
#include "Light.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "imgui.h"

using namespace DirectX;

// ───────────────────────────────────────────────────────────
// 프레임 렌더 루프 — 입력 → 스키닝 → AS 재빌드 → DDGI 디스패치 → 라스터(모델/바닥) → Present
// (D3D12Device.cpp 에서 분리)
// ───────────────────────────────────────────────────────────
void D3D12Device::Render()
{
	GET_SINGLE(TimeManager)->Update();   // 실측 델타타임/FPS
	GET_SINGLE(InputManager)->Update();  // 키/마우스 상태 스냅샷
	const float dt = DT;

	_time += dt;
	if (!_animPaused) _animTimeAcc += dt * _animSpeed; // 애니 재생/속도
	if (_turntable) _turnAngle += dt * _turnSpeed;     // U14 턴테이블
	UpdateParticles(dt);                               // W1 파티클
	// U4 자동 노출 — 씬 조명 추정 휘도에 눈 적응(프리셋 Day/Night 전환 시 부드럽게 밝기 보정)
	if (_autoExp)
	{
		float sunL = 0.2126f * _sunColor.x + 0.7152f * _sunColor.y + 0.0722f * _sunColor.z;
		float lum = _lightIntensity * sunL * 0.6f + _ambient * 3.0f + (_pointOn ? _pointIntensity * 0.04f : 0.0f) + (_spotOn ? _spotIntensity * 0.03f : 0.0f);
		lum = max(lum, 0.04f);
		float target = max(0.25f, min(4.0f, _expTarget / lum));
		_expScale += (target - _expScale) * 0.04f;
	}
	else _expScale = 1.0f;
	// U18 프레임타임 그래프 갱신
	_frameTimes[_frameIdx % 120] = ImGui::GetIO().Framerate > 0 ? 1000.0f / ImGui::GetIO().Framerate : 0.0f;
	_frameIdx++;
	// 자유 비행 카메라 입력 (Scene 뷰 hover/focus 게이팅, F 포커스 대상 = 모델 AABB, W/E/R → 기즈모)
	FlyCamera::InputCtx cam{};
	cam.hwnd = _hwnd;
	cam.inputAllowed = !_editorReady || _sceneHovered;
	cam.keysAllowed  = !_editorReady || _sceneFocused;
	cam.focusMin = _scene._modelMin; cam.focusMax = _scene._modelMax;
	// 선택 GameObject 가 있으면 그 AABB 로 포커스 대상 변경 (직전 프레임 월드 바운드)
	if (_selectedGO && _selectedGO != _modelObj)
		if (auto mr = _selectedGO->GetMeshRenderer())
		{
			auto& bb = mr->GetBoundingBox();
			cam.focusMin = Vec3{ bb.Center.x - bb.Extents.x, bb.Center.y - bb.Extents.y, bb.Center.z - bb.Extents.z };
			cam.focusMax = Vec3{ bb.Center.x + bb.Extents.x, bb.Center.y + bb.Extents.y, bb.Center.z + bb.Extents.z };
		}
	cam.gizmoOp = &_gizmoOp;
	_camera.Update(dt, cam);
	BuildUI(); // ImGui 패널(CPU) — 카메라/라이팅/GI 파라미터 편집

	// Play 중: 씬 그래프 컴포넌트 Update 틱 (스크립트/게임플레이) — 편집 중엔 정지
	if (_playing && _gameScene) { _gameScene->Update(); _gameScene->LateUpdate(); }

	// ParticleSystem 컴포넌트 시뮬레이션 (편집 중에도 동작 — 에디터 미리보기)
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
			if (auto& o = kv.second; o && o->IsActive())
				if (auto ps = std::dynamic_pointer_cast<ParticleSystem>(o->GetRenderer())) ps->Update(dt);

	// 더블클릭/씬로드 모델 교체 (GPU 유휴 시점)
	if (_wantReload && _pendingModel.empty()) { _wantReload = false; _pendingModel = _scene._modelDir + _scene._modelStem + L".mesh"; } // V1 터레인 토글 등 재생성
	if (!_pendingModel.empty())
	{
		std::wstring path = _pendingModel; _pendingModel.clear();
		_scene.Load(path);
		if (_hasPendingMatrix) { _scene._modelMatrix = _pendingMatrix; _hasPendingMatrix = false; } // 씬로드 트랜스폼 복원
	}

	// Scene 창 크기 변경/렌더 스케일 변경 시 오프스크린 RT 재생성 (전체 플러시로 GPU 유휴)
	UINT tW = (UINT)max(8.0f, _pendingSceneW * _renderScale), tH = (UINT)max(8.0f, _pendingSceneH * _renderScale);
	if (_pendingSceneW && (tW != _sceneW || tH != _sceneH))
		CreateSceneRT(tW, tH);


	// V7 카메라 자동 오빗 (원점 중심)
	if (_camera.orbit) _camera.Orbit(_time);

	// ── 상수버퍼 갱신 (카메라 뷰 + 빛 방향 애니메이션 → RT 그림자 이동) ──
	XMMATRIX model = XMMatrixIdentity();
	XMVECTOR eye = _camera.Eye();
	XMVECTOR fwd = _camera.Forward();
	XMMATRIX view = _camera.View();
	float aspect = float(_sceneW) / float(_sceneH); // 씬 RT 비율 (왜곡 방지)
	XMMATRIX proj = _camera.Proj(aspect); // V7
	XMStoreFloat4x4(&_viewM, view); // ImGuizmo 용 (다음 프레임 BuildUI 에서 사용)
	XMStoreFloat4x4(&_projM, proj);

	// V4 시간대 — 태양 각도 + 하늘/세기 블렌드 (밤→낮→석양)
	if (_todOn)
	{
		float day = sinf(_timeOfDay * 3.14159f); day = max(day, 0.0f);
		_lightAngle = (_timeOfDay - 0.5f) * 2.4f;
		_lightIntensity = 0.15f + day * 1.6f;
		XMFLOAT3 nightZ{ 0.02f,0.03f,0.09f }, dayZ{ 0.13f,0.22f,0.44f };
		XMFLOAT3 nightH{ 0.05f,0.06f,0.12f }, dayH{ 0.52f,0.60f,0.72f };
		float sunset = powf(1.0f - day, 2.0f); // 지평선 부근 주황
		_skyZenith = { nightZ.x + (dayZ.x - nightZ.x) * day, nightZ.y + (dayZ.y - nightZ.y) * day, nightZ.z + (dayZ.z - nightZ.z) * day };
		_skyHorizon = { nightH.x + (dayH.x - nightH.x) * day + sunset * 0.5f, nightH.y + (dayH.y - nightH.y) * day + sunset * 0.18f, nightH.z + (dayH.z - nightH.z) * day };
		_sunColor = { 1.0f, 0.85f + day * 0.11f, 0.55f + day * 0.33f };
	}
	// V14 점광원 오빗
	if (_ptOrbit) { _ptOrbitAng += (1.0f / 60.0f) * _ptOrbitSpeed; _pointPos = { cosf(_ptOrbitAng) * 2.6f, _pointPos.y, sinf(_ptOrbitAng) * 2.6f }; }
	// 빛 방향 — 애니메이션 또는 인스펙터 수동 각도 (그림자/간접광 같이 변함)
	if (_lightAnimate && !_todOn) _lightAngle = _time * 0.6f;
	float a = _lightAngle;

	SyncLights(); // 스칼라 → Light 컴포넌트 (CB 가 컴포넌트에서 읽음)
	auto sunL  = _sunObj  ? _sunObj->GetLight()  : nullptr;
	_flickerV = _flicker ?(0.65f + 0.35f * sinf(_time * 31.0f) * sinf(_time * 7.3f) + 0.1f * sinf(_time * 53.0f)) : 1.0f; // W9

	SceneCB cb;
	XMStoreFloat4x4(&cb.mvp, model * view * proj); // row_major HLSL → 전치 불필요
	XMStoreFloat4x4(&cb.model, model);
	cb.lightDir = sunL ? XMFLOAT4(sunL->_direction.x, sunL->_direction.y, sunL->_direction.z, sunL->_intensity)
	                   : XMFLOAT4(cosf(a) * 0.6f, -1.0f, sinf(a) * 0.6f, _lightIntensity); // w=세기 (Light 컴포넌트 소스)
	XMStoreFloat4(&cb.camPos, eye);
	cb.gridMin  = XMFLOAT4(-6.5f, 0.2f, -6.5f, 0.f);
	cb.gridMax  = XMFLOAT4( 6.5f, 4.0f,  6.5f, 0.f);
	cb.gridDim  = XMFLOAT4(float(Ddgi::PROBE_X), float(Ddgi::PROBE_Y), float(Ddgi::PROBE_Z), 128.f); // 128 rays/probe
	cb.giParams = XMFLOAT4(_giStrength, _time * 60.f, _ambient, 0.f); // GI세기 / frame / 앰비언트
	XMStoreFloat4x4(&cb.invVP, XMMatrixInverse(nullptr, view * proj)); // 스카이 레이 복원
	// ── 점/스팟 라이트: 씬의 모든 Light 컴포넌트에서 수집 (동적 추가 라이트 포함, 점=최대4 스팟=1) ──
	int ptN = 0; XMFLOAT4 ptPosA[4] = {}, ptColA[4] = {};
	bool haveSpot = false; XMFLOAT4 spotP{}, spotD{ 0,-1,0,0.5f }, spotC{};
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive()) continue;
			auto l = o->GetLight(); if (!l || !l->_enabled) continue;
			auto t = o->GetTransform();
			Vec3 lp = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
			if (l->_lightType == LightType::Point && ptN < 4)
			{
				float in = l->_intensity * _flickerV;
				ptPosA[ptN] = XMFLOAT4(lp.x, lp.y, lp.z, l->_range);
				ptColA[ptN] = XMFLOAT4(l->_color.x * in, l->_color.y * in, l->_color.z * in, 1.f);
				++ptN;
			}
			else if (l->_lightType == LightType::Spot && !haveSpot)
			{
				XMVECTOR sd2 = XMVector3Normalize(XMLoadFloat3(&l->_direction));
				XMFLOAT3 sdn2; XMStoreFloat3(&sdn2, sd2);
				spotP = XMFLOAT4(lp.x, lp.y, lp.z, l->_range);
				spotD = XMFLOAT4(sdn2.x, sdn2.y, sdn2.z, cosf(XMConvertToRadians(l->_spotAngleDeg)));
				spotC = XMFLOAT4(l->_color.x * l->_intensity, l->_color.y * l->_intensity, l->_color.z * l->_intensity, 1.f);
				haveSpot = true;
			}
		}
	cb.pointPos   = ptN > 0 ? ptPosA[0] : XMFLOAT4(0, 0, 0, 0); // 단일 참조(스크린 갓레이 등) 호환
	cb.pointColor = ptN > 0 ? ptColA[0] : XMFLOAT4(0, 0, 0, 0);
	cb.matParams  = XMFLOAT4(_matMetallic, _matRoughness, _matEmissive, _matTint);
	cb.sunColor   = sunL ? XMFLOAT4(sunL->_color.x, sunL->_color.y, sunL->_color.z, _envIntensity)
	                     : XMFLOAT4(_sunColor.x, _sunColor.y, _sunColor.z, _envIntensity);
	cb.fog        = XMFLOAT4(_fogColor.x, _fogColor.y, _fogColor.z, _fogDensity);
	cb.grade      = XMFLOAT4(_contrast, _saturation, _temperature, _vignette);
	cb.skyZenith  = XMFLOAT4(_skyZenith.x, _skyZenith.y, _skyZenith.z, _shadowSoft);
	cb.skyHorizon = XMFLOAT4(_skyHorizon.x, _skyHorizon.y, _skyHorizon.z, _sunSize);
	cb.dbg        = XMFLOAT4(float(_debugView), _probeViz ? 1.f : 0.f, float(_tonemapOp), _reflectOn ? _reflectStrength : 0.f);
	// 스팟 라이트 (씬 수집 결과 — haveSpot 이면 첫 스팟 컴포넌트)
	cb.spotPos    = spotP;
	cb.spotDir    = spotD;
	cb.spotColor  = haveSpot ? spotC : XMFLOAT4(0, 0, 0, 0);
	cb.tint       = XMFLOAT4(_diffuseTint.x * _matTint, _diffuseTint.y * _matTint, _diffuseTint.z * _matTint, _floorRough);
	cb.floorMat   = XMFLOAT4(_floorColor.x, _floorColor.y, _floorColor.z, _floorMetallic);
	cb.ao         = XMFLOAT4(_aoOn ? 1.f : 0.f, _aoIntensity, _aoRadius, 0.f);
	cb.shade      = XMFLOAT4(float(_toonLevels), _rimPower, _normalIntensity, _checker ? 1.f : 0.f);
	cb.rimColor   = XMFLOAT4(_rimColor.x, _rimColor.y, _rimColor.z, 0.f);
	cb.gridParams = XMFLOAT4(_gridCell, _gridFade, float(_bgMode), 0.f);
	cb.outline    = XMFLOAT4(_outlineColor.x, _outlineColor.y, _outlineColor.z, _outlineThick);
	cb.decal      = XMFLOAT4(_decalPos.x, _decalPos.y, _decalPos.z, _decalOn ? _decalRadius : 0.f); // W2
	cb.decalCol   = XMFLOAT4(_decalColor.x, _decalColor.y, _decalColor.z, _cloudAmt);                // W2/W3
	cb.extra      = XMFLOAT4(_shadowStrength, _hemiAmbient, _stars ? 1.f : 0.f, 0.f);                // W6/W7/W8
	// 다중 점광원 — 씬에서 수집한 ptN 개 (셰이더 gPtPos/gPtCol[4])
	for (int i = 0; i < 4; ++i) { cb.ptPos[i] = (i < ptN) ? ptPosA[i] : XMFLOAT4(0,0,0,0); cb.ptCol[i] = (i < ptN) ? ptColA[i] : XMFLOAT4(0,0,0,0); }
	memcpy(_cbMapped[_frameIndex], &cb, sizeof(cb));

	// ── 모델 갱신: 스키닝(or 바인드) + 기즈모 트랜스폼 적용 → VB (GPU 유휴, 전체 플러시) ──
	_scene.UpdateAnimation(_animTimeAcc, _turntable, _turnAngle);

	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), nullptr), "CmdList Reset");

	// ── RT 가속구조: 모든 렌더러 BLAS + 통합 TLAS (모델/바닥 + 스폰 메시 + 애니) ──
	{
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> rtInst;
		_scene.RecordBuildModelBLAS(_cmdList.Get()); // 모델+바닥 BLAS
		rtInst.push_back(RtBlas::IdentityInstance(_scene._blas->GetGPUVirtualAddress()));
		if (_gameScene)
			for (auto& kv : _gameScene->GetCreatedObjects())
			{
				auto& o = kv.second;
				if (!o || !o->IsActive() || o == _modelObj) continue;
				if (rtInst.size() >= ModelScene::MAX_INSTANCES) break;
				if (auto mr = o->GetMeshRenderer())
				{
					mr->UpdateWorld(); mr->RecordBuildBLAS(_cmdList.Get());
					if (mr->BlasAddr()) rtInst.push_back(RtBlas::IdentityInstance(mr->BlasAddr()));
				}
				else if (auto an = o->GetModelAnimator())
				{
					an->UpdateWorld(); an->RecordBuildBLAS(_cmdList.Get());
					if (an->BlasAddr()) rtInst.push_back(RtBlas::IdentityInstance(an->BlasAddr()));
				}
			}
		// 모든 BLAS 빌드 완료 후 UAV 배리어(전체) → 통합 TLAS
		D3D12_RESOURCE_BARRIER uav{}; uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav.UAV.pResource = nullptr;
		_cmdList->ResourceBarrier(1, &uav);
		_scene.BuildTLAS(_cmdList.Get(), rtInst);
	}

	// ── DDGI: 프로브 irradiance 갱신 (컴퓨트 RT) — 라스터 전에 ──
	_ddgi.Dispatch(_cmdList.Get(), _cb[_frameIndex]->GetGPUVirtualAddress(),
	               _scene._tlas->GetGPUVirtualAddress(), _scene._vb->GetGPUVirtualAddress(), _scene._ib->GetGPUVirtualAddress());

	// ── 씬 3D → 오프스크린 RT (Scene 도킹 탭 이미지) ──
	Transition(_sceneRT.Get(), _sceneRTState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Transition(_sceneDepth.Get(), _sceneDepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE); // DOF 샘플 후 복귀

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _sceneRtvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _sceneDsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	float clear[4] = { _bgColor.x, _bgColor.y, _bgColor.z, 1.0f }; // V17 배경색
	_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT vp{ 0.f, 0.f, float(_sceneW), float(_sceneH), 0.f, 1.f };
	D3D12_RECT sc{ 0, 0, LONG(_sceneW), LONG(_sceneH) };
	_cmdList->RSSetViewports(1, &vp);
	_cmdList->RSSetScissorRects(1, &sc);

	// ── 카메라가 씬 렌더러를 큐별 분류 (1회) — Background/Opaque/Transparent 버킷 ──
	RenderContext ctx{};
	ctx.cmd = _cmdList.Get();
	XMStoreFloat4x4(&ctx.view, view);
	XMStoreFloat4x4(&ctx.proj, proj);
	ctx.deferredPass = false;
	shared_ptr<Camera> sceneCam;
	if (auto camObj = _gameScene ? _gameScene->GetMainCamera() : nullptr)
	{
		sceneCam = camObj->GetCamera();
		sceneCam->SetView(ctx.view);
		sceneCam->SetProjection(ctx.proj);
		sceneCam->SortGameObject();
	}
	auto drawBucket = [&](vector<shared_ptr<GameObject>>& bucket, bool cull)
	{
		for (auto& obj : bucket)
		{
			if (!obj || !obj->IsActive()) continue;
			auto r = obj->GetRenderer(); if (!r) continue;
			if (cull && sceneCam && !sceneCam->GetFrustum().Contains(r->GetBoundingBox())) continue;
			r->Draw(ctx);
		}
	};

	// ── 배경(스카이박스) — SkyRenderer (Background 큐, 컬링 제외) ──
	if (sceneCam) drawBucket(sceneCam->GetVecBackground(), false);

	// ── 선택 아웃라인 (모델 선택 시, 인버티드 헐 → 모델이 위에 덮어 림만 남음) ──
	if (_sel == SelEntity::Model && _scene._modelIndexCount > 0)
	{
		_cmdList->SetPipelineState(_outlinePSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &_scene._vbv);
		_cmdList->IASetIndexBuffer(&_scene._ibv);
		_cmdList->DrawIndexedInstanced(_scene._modelIndexCount, 1, 0, 0, 0);
	}

	// ── 불투명 (모델+바닥+정적메시) — Opaque 큐 (절두체 컬링 옵션) ──
	if (sceneCam) drawBucket(sceneCam->GetVecOpaque(), _frustumCull);

	// ── 그리드 — GridRenderer (Transparent 큐, 컬링 제외) ──
	if (sceneCam) drawBucket(sceneCam->GetVecTransparent(), false);

	// ── 디버그 라인 (본/AABB/콘/아이콘) ──
	DrawDebugLines();

	// ── DDGI 프로브 시각화 (점) ──
	if (_probeViz)
	{
		_cmdList->SetPipelineState(_probePSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->SetGraphicsRootShaderResourceView(2, _ddgi.ProbesAddr());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		_cmdList->DrawInstanced(Ddgi::PROBE_COUNT, 1, 0, 0);
	}

	// ── 블룸 (브라이트패스 → BlurH → BlurV, 반해상도) ── PostFX 가 처리
	Transition(_sceneRT.Get(), _sceneRTState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	bool bloomActive = _bloomOn && _postfx.Ready();
	if (bloomActive) _postfx.Bloom(_cmdList.Get(), _bloomThreshold);

	// ── 톤맵 (HDR 씬 RT → LDR RT, ACES + 노출 + 감마 + 블룸 + DOF/갓레이) ── PostFX 가 처리
	Transition(_sceneDepth.Get(), _sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // DOF 입력

	// 태양 화면 위치(갓레이) — 카메라 의존이라 여기서 계산해 전달
	XMVECTOR sunW = XMVectorAdd(eye, XMVectorScale(XMVector3Normalize(XMVectorNegate(XMLoadFloat4(&cb.lightDir))), 500.0f));
	XMVECTOR sc4 = XMVector4Transform(XMVectorSetW(sunW, 1.0f), view * proj);
	float sw = XMVectorGetW(sc4); float sunSX = 0.5f, sunSY = 0.5f; bool sunVis = sw > 0.0f;
	if (sunVis) { sunSX = XMVectorGetX(sc4) / sw * 0.5f + 0.5f; sunSY = -XMVectorGetY(sc4) / sw * 0.5f + 0.5f; }

	PostFX::TonemapParams tp{};
	tp.exposure = _exposure * powf(2.0f, _ev); tp.bloomIntensity = _bloomIntensity; tp.bloomEnabled = bloomActive; tp.tonemapOp = _tonemapOp;
	tp.contrast = _contrast; tp.saturation = _saturation; tp.temperature = _temperature; tp.vignette = _vignette;
	tp.chroma = _chroma; tp.grain = _grain; tp.sharpen = _sharpen; tp.time = _time * 60.0f; tp.expScale = _expScale;
	tp.sunSX = sunSX; tp.sunSY = sunSY; tp.sunVisible = sunVis;
	tp.volStrength = _volStrength; tp.dofFocus = _dofFocus; tp.dofRange = _dofRange; tp.dofOn = _dofOn; tp.volOn = _volOn;
	tp.lensDistort = _lensDistort; tp.posterize = _posterize; tp.anamorphic = _anamorphic; tp.filterMode = _filterMode;
	_postfx.Tonemap(_cmdList.Get(), tp);

	// ── FXAA (LDR → LDR2), 표시 텍스처 선택 ──
	ID3D12Resource* displayRes = _postfx.Fxaa(_cmdList.Get(), _fxaaOn);
	_sceneTexId = _imgui.SetSceneTexture(displayRes); // FXAA on→LDR2, off→LDR

	// ── 백버퍼: 에디터 UI(도킹+씬 이미지) ImGui 드로우 ──
	D3D12_RESOURCE_BARRIER toRT{};
	toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toRT.Transition.pResource = _renderTargets[_frameIndex].Get();
	toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_cmdList->ResourceBarrier(1, &toRT);

	D3D12_CPU_DESCRIPTOR_HANDLE bbRtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	bbRtv.ptr += SIZE_T(_frameIndex) * _rtvDescSize;
	_cmdList->OMSetRenderTargets(1, &bbRtv, FALSE, nullptr);
	float bbClear[4] = { 0.02f, 0.02f, 0.03f, 1.0f };
	_cmdList->ClearRenderTargetView(bbRtv, bbClear, 0, nullptr);

	_imgui.Render(_cmdList.Get(), _frameIndex);

	D3D12_RESOURCE_BARRIER toPresent = toRT;
	toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &toPresent);

	ThrowIfFailed(_cmdList->Close(), "CmdList Close");
	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_queue->ExecuteCommandLists(1, lists);

	HRESULT hrPresent = _swapChain->Present(1, 0);
	if (hrPresent == DXGI_ERROR_DEVICE_REMOVED || hrPresent == DXGI_ERROR_DEVICE_RESET)
	{ DumpDeviceRemoved(); return; } // 어떤 GPU 명령에서 죽었는지 DRED 덤프
	ThrowIfFailed(hrPresent, "Present");
	MoveToNextFrame();

	if (_wantShot) { _wantShot = false; SaveScreenshot(); if (_hiresShot) { _renderScale = 1.0f; _hiresShot = false; } } // GPU 유휴 시점 리드백
}
