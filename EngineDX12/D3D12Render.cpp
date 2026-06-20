#include "D3D12Device.h"
#include "Renderer.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Terrain.h"
#include "Billboard.h"
#include "ParticleSystem.h"
#include "RtBlas.h"
#include "Camera.h"
#include "Light.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "imgui.h"

using namespace DirectX;

// RT 집계 지오메트리 — TLAS 인스턴스 순서대로 (월드정점, 인덱스) 를 한 버퍼로 모으고
// 인스턴스별 {vbBase, ibBase} 를 _rtMeta 에 기록. gather 셰이더가 InstanceID 로 자기 지오메트리 페치.
void D3D12Device::BuildRtGeometry(const std::vector<std::pair<const std::vector<Vtx>*, const std::vector<uint32>*>>& geom)
{
	uint32 totV = 0, totI = 0;
	for (auto& g : geom) { totV += (uint32)g.first->size(); totI += (uint32)g.second->size(); }
	if (totV == 0 || totI == 0) return;
	auto ensure = [&](ComPtr<ID3D12Resource>& buf, void*& mapped, UINT& cap, UINT need)
	{
		if (buf && cap >= need) return;
		UINT n = max(need, cap ? cap * 2 : need);
		buf = CreateUploadBuffer(nullptr, n);
		D3D12_RANGE nr{ 0, 0 }; buf->Map(0, &nr, &mapped);
		cap = n;
	};
	ensure(_rtVB, _rtVBMapped, _rtVBCap, totV * (UINT)sizeof(Vtx));
	ensure(_rtIB, _rtIBMapped, _rtIBCap, totI * (UINT)sizeof(uint32));
	ensure(_rtMeta, _rtMetaMapped, _rtMetaCap, (UINT)geom.size() * 2 * (UINT)sizeof(uint32));
	Vtx* vdst = (Vtx*)_rtVBMapped; uint32* idst = (uint32*)_rtIBMapped; uint32* mdst = (uint32*)_rtMetaMapped;
	uint32 vbBase = 0, ibBase = 0;
	for (size_t k = 0; k < geom.size(); ++k)
	{
		const auto& V = *geom[k].first; const auto& I = *geom[k].second;
		mdst[k * 2 + 0] = vbBase; mdst[k * 2 + 1] = ibBase;
		if (!V.empty()) memcpy(vdst + vbBase, V.data(), V.size() * sizeof(Vtx));
		if (!I.empty()) memcpy(idst + ibBase, I.data(), I.size() * sizeof(uint32));
		vbBase += (uint32)V.size(); ibBase += (uint32)I.size();
	}
}

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
		_scene.Load(path); // 바닥 재생성(터레인 토글/그라운드 사이즈)
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
	int ptN = 0; XMFLOAT4 ptPosA[16] = {}, ptColA[16] = {};
	bool haveSpot = false; XMFLOAT4 spotP{}, spotD{ 0,-1,0,0.5f }, spotC{};
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive()) continue;
			auto l = o->GetLight(); if (!l || !l->_enabled) continue;
			auto t = o->GetTransform();
			Vec3 lp = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
			if (l->_lightType == LightType::Point && ptN < 16)
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
	cb.matParams  = XMFLOAT4(0, 0.5f, 0, 1); // (gMatParams 미사용 — 모델 분리 후 per-object 루트상수가 머티리얼 담당)
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
	cb.tint       = XMFLOAT4(0, 0, 0, _floorRough); // gTint.w=바닥 거칠기만 사용(rgb 미사용)
	cb.floorMat   = XMFLOAT4(_floorColor.x, _floorColor.y, _floorColor.z, _floorMetallic);
	cb.ao         = XMFLOAT4(_aoOn ? 1.f : 0.f, _aoIntensity, _aoRadius, 0.f);
	cb.shade      = XMFLOAT4(float(_toonLevels), _rimPower, _normalIntensity, _checker ? 1.f : 0.f);
	cb.rimColor   = XMFLOAT4(_rimColor.x, _rimColor.y, _rimColor.z, 0.f);
	cb.gridParams = XMFLOAT4(_gridCell, _gridFade, float(_bgMode), 0.f);
	cb.outline    = XMFLOAT4(_outlineColor.x, _outlineColor.y, _outlineColor.z, _outlineThick);
	cb.decal      = XMFLOAT4(_decalPos.x, _decalPos.y, _decalPos.z, _decalOn ? _decalRadius : 0.f); // W2
	cb.decalCol   = XMFLOAT4(_decalColor.x, _decalColor.y, _decalColor.z, _cloudAmt);                // W2/W3
	cb.extra      = XMFLOAT4(_shadowStrength, _hemiAmbient, _stars ? 1.f : 0.f, (_skyCubemapOn && _skyCube) ? 1.f : 0.f); // W6/W7/W8 + w=큐브맵 스카이
	// 다중 점광원 — 씬에서 수집한 ptN 개 (셰이더 gPtPos/gPtCol[4])
	for (int i = 0; i < 16; ++i) { cb.ptPos[i] = (i < ptN) ? ptPosA[i] : XMFLOAT4(0,0,0,0); cb.ptCol[i] = (i < ptN) ? ptColA[i] : XMFLOAT4(0,0,0,0); }
	cb.fog2 = XMFLOAT4(_fogHeight, _fogFalloff, _heightFog ? 1.f : 0.f, 0.f);
	for (int i = 0; i < 8; ++i)
	{
		bool on = i < (int)_decals.size();
		cb.decalArr[i]    = on ? XMFLOAT4(_decals[i].pos.x, _decals[i].pos.y, _decals[i].pos.z, _decals[i].radius) : XMFLOAT4(0,0,0,0);
		cb.decalColArr[i] = on ? XMFLOAT4(_decals[i].color.x, _decals[i].color.y, _decals[i].color.z, 1.f) : XMFLOAT4(0,0,0,0);
	}
	memcpy(_cbMapped[_frameIndex], &cb, sizeof(cb));
	_cbCache = cb; // 게임 뷰 패스 베이스(카메라 필드만 교체)

	// (_scene 는 바닥 전용 — 정적 VB 는 생성 시 1회 업로드. 모델 스키닝은 ModelAnimator GameObject 가 자체 처리)
	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), nullptr), "CmdList Reset");

	// ── RT 가속구조: 모든 렌더러 BLAS + 통합 TLAS (모델/바닥 + 스폰 메시 + 애니) ──
	// 인스턴스별 지오메트리 페치(per-instance geometry) — 각 인스턴스가 자기 월드 정점/인덱스를 가지므로
	// 더 이상 전역 모델 VB 와 인덱스 수가 맞을 필요 없음(OOB 가드 제거). geomList 는 rtInst 와 동순서.
	{
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> rtInst;
		std::vector<std::pair<const std::vector<Vtx>*, const std::vector<uint32>*>> geomList;
		auto addInst = [&](D3D12_GPU_VIRTUAL_ADDRESS blas, const std::vector<Vtx>* v, const std::vector<uint32>* i)
		{
			auto d = RtBlas::IdentityInstance(blas);
			d.InstanceID = (UINT)rtInst.size(); // gather 셰이더가 이 ID 로 geomList 메타 조회
			rtInst.push_back(d); geomList.push_back({ v, i });
		};
		_scene.RecordBuildModelBLAS(_cmdList.Get()); // 모델+바닥 BLAS (인스턴스 0)
		addInst(_scene._blas->GetGPUVirtualAddress(), &_scene._cpuVerts, &_scene._cpuIndices);
		if (_gameScene)
			for (auto& kv : _gameScene->GetCreatedObjects())
			{
				auto& o = kv.second;
				if (!o || !o->IsActive() || o == _modelObj) continue;
				if (rtInst.size() >= ModelScene::MAX_INSTANCES) break;
				// 터레인은 여전히 RT 제외(수만 삼각형 — 매 프레임 집계 비용/메모리). 래스터는 정상.
				if (o->GetTerrain()) continue;
				if (auto mr = o->GetMeshRenderer())
				{
					mr->UpdateWorld(); mr->RecordBuildBLAS(_cmdList.Get());
					if (mr->BlasAddr()) addInst(mr->BlasAddr(), &mr->GetWorldVerts(), &mr->GetLocalIndices());
				}
				else if (auto an = o->GetModelAnimator())
				{
					an->UpdateWorld(); an->RecordBuildBLAS(_cmdList.Get());
					if (an->BlasAddr()) addInst(an->BlasAddr(), &an->GetWorldVerts(), &an->GetIndices());
				}
			}
		BuildRtGeometry(geomList); // 집계 정점/인덱스/메타 채움 (rtInst 동순서)
		// 모든 BLAS 빌드 완료 후 UAV 배리어(전체) → 통합 TLAS
		D3D12_RESOURCE_BARRIER uav{}; uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav.UAV.pResource = nullptr;
		_cmdList->ResourceBarrier(1, &uav);
		_scene.BuildTLAS(_cmdList.Get(), rtInst);
	}

	// ── DDGI: 프로브 irradiance 갱신 (컴퓨트 RT) — 집계 VB/IB + 인스턴스 메타로 per-instance 페치 ──
	_ddgi.Dispatch(_cmdList.Get(), _cb[_frameIndex]->GetGPUVirtualAddress(),
	               _scene._tlas->GetGPUVirtualAddress(),
	               (_rtVB ? _rtVB->GetGPUVirtualAddress() : _scene._vb->GetGPUVirtualAddress()),
	               (_rtIB ? _rtIB->GetGPUVirtualAddress() : _scene._ib->GetGPUVirtualAddress()),
	               (_rtMeta ? _rtMeta->GetGPUVirtualAddress() : _scene._vb->GetGPUVirtualAddress()));

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
	ctx.cb = _cb[_frameIndex]->GetGPUVirtualAddress(); // 에디터 카메라 CB
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
			if (_tessTerrain && obj->GetTerrain()) continue; // 테셀레이션 ON 시 터레인 메시 드로우 생략(테셀 패스가 대체)
			if (cull && sceneCam && !sceneCam->GetFrustum().Contains(r->GetBoundingBox())) continue;
			r->Draw(ctx);
		}
	};

	// ── 배경(스카이박스) — SkyRenderer (Background 큐, 컬링 제외) ──
	if (sceneCam) drawBucket(sceneCam->GetVecBackground(), false);

	// ── 선택 아웃라인 (인버티드 헐 → 이후 불투명 패스가 위를 덮어 림만 남음) ──
	// 선택된 GameObject 렌더러 + 멀티셀렉트 전부 아웃라인.
	auto outlineR = (_selectedGO && !_selectedGO->IsEditorInternal()) ? _selectedGO->GetRenderer() : nullptr;
	if (outlineR || !_selIds.empty())
	{
		_cmdList->SetPipelineState(_outlinePSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		if (outlineR) outlineR->RecordOutline(_cmdList.Get()); // primary
		// 멀티셀렉트 — 추가 선택 전부 아웃라인
		for (int64 id : _selIds)
			if (auto o = _gameScene ? _gameScene->GetCreatedObject(id) : nullptr)
				if (auto r = o->GetRenderer()) r->RecordOutline(_cmdList.Get());
	}

	// ── 불투명 (모델+바닥+정적메시) — Opaque 큐 (절두체 컬링 옵션) ──
	if (sceneCam) drawBucket(sceneCam->GetVecOpaque(), _frustumCull);

	// ── 터레인 GPU 테셀레이션 (토글 ON 시) ──
	RenderTessTerrain(ctx);

	// ── 물 평면 (토글 ON 시) ──
	RenderWater(ctx);

	// ── 그리드 — GridRenderer (Transparent 큐, 컬링 제외) ──
	if (sceneCam) drawBucket(sceneCam->GetVecTransparent(), false);

	// ── GPU 인스턴스드 빌보드 파티클 (가산 블렌드) ──
	RenderParticles(ctx);

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

	// ── Game 뷰: 게임 카메라 시점 별도 RT (DDGI/TLAS 재사용) ──
	RenderGameView();

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

// ── Game 뷰 — 배치된 게임 카메라(비-에디터 Camera) 시점을 별도 RT 에 렌더 (DDGI/TLAS 재사용) ──
void D3D12Device::RenderGameView()
{
	using namespace DirectX;
	if (!_gameWindowOpen || !_gameScene) return;

	// 게임 카메라 = 비-에디터 내부 Camera GameObject (첫 번째)
	shared_ptr<GameObject> camObj;
	for (auto& kv : _gameScene->GetCreatedObjects())
		if (auto& o = kv.second; o && o->IsActive() && !o->IsEditorInternal() && o->GetCamera()) { camObj = o; break; }

	if (_pendingGameW && (_pendingGameW != _gameW || _pendingGameH != _gameH)) CreateGameRT(_pendingGameW, _pendingGameH);
	if (!_gameRT) return;

	Transition(_gameRT.Get(), _gameRTState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Transition(_gameDepth.Get(), _gameDepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _gameRtvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _gameDsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	float clr[4] = { 0.02f, 0.02f, 0.03f, 1.f };
	_cmdList->ClearRenderTargetView(rtv, clr, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	D3D12_VIEWPORT vp{ 0, 0, float(_gameW), float(_gameH), 0, 1 }; D3D12_RECT scr{ 0, 0, (LONG)_gameW, (LONG)_gameH };
	_cmdList->RSSetViewports(1, &vp); _cmdList->RSSetScissorRects(1, &scr);

	if (camObj)
	{
		auto t = camObj->GetTransform(); auto cam = camObj->GetCamera();
		Matrix wm = t->GetWorldMatrix(); XMMATRIX W = XMLoadFloat4x4(&wm);
		XMMATRIX view = XMMatrixInverse(nullptr, W);
		float aspect = float(_gameW) / float(_gameH);
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(cam->_fov), aspect, cam->_near, cam->_far);

		SceneCB g = _cbCache; // 에디터 CB 베이스 + 카메라 필드 교체
		XMStoreFloat4x4(&g.mvp, view * proj);
		XMVECTOR eye = W.r[3]; XMStoreFloat4(&g.camPos, eye);
		XMStoreFloat4x4(&g.invVP, XMMatrixInverse(nullptr, view * proj));
		memcpy(_gameCBMapped, &g, sizeof(g));

		RenderContext ctx{}; ctx.cmd = _cmdList.Get(); ctx.cb = _gameCB->GetGPUVirtualAddress();
		XMStoreFloat4x4(&ctx.view, view); XMStoreFloat4x4(&ctx.proj, proj);
		cam->SetView(ctx.view); cam->SetProjection(ctx.proj); cam->SortGameObject();
		auto draw = [&](vector<shared_ptr<GameObject>>& b) { for (auto& o : b) { if (!o || !o->IsActive()) continue; auto r = o->GetRenderer(); if (r) r->Draw(ctx); } };
		draw(cam->GetVecBackground()); // 스카이
		draw(cam->GetVecOpaque());     // 모델/메시/애니 (그리드=Transparent 생략)
	}

	// 포스트 (블룸/톤맵/FXAA — 게임 전용 PostFX)
	Transition(_gameRT.Get(), _gameRTState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	bool bloomA = _bloomOn && _gamePostfx.Ready();
	if (bloomA) _gamePostfx.Bloom(_cmdList.Get(), _bloomThreshold);
	Transition(_gameDepth.Get(), _gameDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PostFX::TonemapParams tp{};
	tp.exposure = _exposure * powf(2.f, _ev); tp.bloomIntensity = _bloomIntensity; tp.bloomEnabled = bloomA; tp.tonemapOp = _tonemapOp;
	tp.contrast = _contrast; tp.saturation = _saturation; tp.temperature = _temperature; tp.vignette = _vignette;
	tp.time = _time * 60.f; tp.expScale = _expScale;
	_gamePostfx.Tonemap(_cmdList.Get(), tp);
	ID3D12Resource* disp = _gamePostfx.Fxaa(_cmdList.Get(), _fxaaOn);
	_gameTexId = _imgui.SetGameTexture(disp);
}

