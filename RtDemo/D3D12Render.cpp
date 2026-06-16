#include "D3D12Device.h"
#include "imgui.h"

using namespace DirectX;

// 카메라 시선 벡터 (yaw=Y축, pitch=X축, LH: yaw=0 → +Z)
static XMVECTOR CamForward(float yaw, float pitch)
{
	float cp = cosf(pitch);
	return XMVectorSet(cp * sinf(yaw), sinf(pitch), cp * cosf(yaw), 0.f);
}

// ───────────────────────────────────────────────────────────
// 자유 비행 카메라 — WASD 이동, 우클릭 드래그 마우스 룩, Q/E 상하, Shift 가속
// ───────────────────────────────────────────────────────────
void D3D12Device::UpdateCamera(float dt)
{
	// Scene 뷰포트 위에서만 카메라 조작 (다른 패널 위에선 무시, 드래그 중이면 유지)
	bool sceneInput = !_editorReady || _sceneHovered;
	bool sceneKey   = !_editorReady || _sceneFocused;

	// 마우스 룩 (우클릭 유지 동안 — 윈도우 중앙 재고정 방식으로 델타 측정)
	bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	bool focused = (GetForegroundWindow() == _hwnd);
	if (rmb && focused && (sceneInput || _rmbDown))
	{
		POINT cur; GetCursorPos(&cur);
		if (!_rmbDown) { _lastCursor = cur; _rmbDown = true; ShowCursor(FALSE); }
		float dx = float(cur.x - _lastCursor.x), dy = float(cur.y - _lastCursor.y);
		const float sens = 0.0032f;
		_camYaw   += dx * sens;
		_camPitch -= dy * sens;
		_camPitch = max(-1.553f, min(1.553f, _camPitch)); // ±89°
		// 커서를 윈도우 중앙으로 재고정 (무한 회전)
		RECT rc; GetClientRect(_hwnd, &rc);
		POINT c{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
		ClientToScreen(_hwnd, &c);
		SetCursorPos(c.x, c.y);
		_lastCursor = c;
	}
	else if (_rmbDown) { _rmbDown = false; ShowCursor(TRUE); }

	if (!focused) return;

	XMVECTOR fwd = CamForward(_camYaw, _camPitch);
	XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, fwd));

	// 기즈모 단축키 + F 포커스 (플라이 아닐 때, Scene 포커스 시)
	if (sceneKey && !_rmbDown)
	{
		if (GetAsyncKeyState('W') & 0x8000) _gizmoOp = 7;   // translate
		if (GetAsyncKeyState('E') & 0x8000) _gizmoOp = 120; // rotate
		if (GetAsyncKeyState('R') & 0x8000) _gizmoOp = 896; // scale
		if (GetAsyncKeyState('F') & 0x8000)
		{
			XMVECTOR c = XMVectorScale(XMVectorAdd(XMLoadFloat3(&_modelMin), XMLoadFloat3(&_modelMax)), 0.5f);
			XMVECTOR ext = XMVectorSubtract(XMLoadFloat3(&_modelMax), XMLoadFloat3(&_modelMin));
			float rad = XMVectorGetX(XMVector3Length(ext)) * 0.5f + 0.5f;
			XMStoreFloat3(&_camPos, XMVectorSubtract(c, XMVectorScale(fwd, rad * 2.6f)));
		}
		return; // 플라이 아니면 이동 안 함 (단축키 우선)
	}

	if (!_rmbDown) return; // 이동은 우클릭 플라이 모드에서만

	float speed = _moveSpeed * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? _fastMul : 1.0f) * dt;
	XMVECTOR pos = XMLoadFloat3(&_camPos);
	if (GetAsyncKeyState('W') & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(fwd, speed));
	if (GetAsyncKeyState('S') & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(fwd, speed));
	if (GetAsyncKeyState('D') & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(right, speed));
	if (GetAsyncKeyState('A') & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(right, speed));
	if (GetAsyncKeyState('E') & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(up, speed));
	if (GetAsyncKeyState('Q') & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(up, speed));
	XMStoreFloat3(&_camPos, pos);
}

// ───────────────────────────────────────────────────────────
// 프레임 렌더 루프 — 입력 → 스키닝 → AS 재빌드 → DDGI 디스패치 → 라스터(모델/바닥) → Present
// (D3D12Device.cpp 에서 분리)
// ───────────────────────────────────────────────────────────
void D3D12Device::Render()
{
	_time += 1.0f / 60.0f;
	if (!_animPaused) _animTimeAcc += (1.0f / 60.0f) * _animSpeed; // 애니 재생/속도
	if (_turntable) _turnAngle += (1.0f / 60.0f) * _turnSpeed;     // U14 턴테이블
	UpdateParticles(1.0f / 60.0f);                                // W1 파티클
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
	UpdateCamera(1.0f / 60.0f);
	BuildUI(); // ImGui 패널(CPU) — 카메라/라이팅/GI 파라미터 편집

	// 더블클릭/씬로드 모델 교체 (GPU 유휴 시점)
	if (_wantReload && _pendingModel.empty()) { _wantReload = false; _pendingModel = _modelDir + _modelStem + L".mesh"; } // V1 터레인 토글 등 재생성
	if (!_pendingModel.empty())
	{
		std::wstring path = _pendingModel; _pendingModel.clear();
		LoadModelFromFile(path);
		if (_hasPendingMatrix) { _modelMatrix = _pendingMatrix; _hasPendingMatrix = false; } // 씬로드 트랜스폼 복원
	}

	// Scene 창 크기 변경/렌더 스케일 변경 시 오프스크린 RT 재생성 (전체 플러시로 GPU 유휴)
	UINT tW = (UINT)max(8.0f, _pendingSceneW * _renderScale), tH = (UINT)max(8.0f, _pendingSceneH * _renderScale);
	if (_pendingSceneW && (tW != _sceneW || tH != _sceneH))
		CreateSceneRT(tW, tH);


	// V7 카메라 자동 오빗 (원점 중심)
	if (_orbit)
	{
		float ang = _time * 0.3f;
		_camPos = { cosf(ang) * 6.0f, 3.0f, sinf(ang) * 6.0f };
		XMVECTOR d = XMVector3Normalize(XMVectorSubtract(XMVectorSet(0, 1.0f, 0, 0), XMLoadFloat3(&_camPos)));
		XMFLOAT3 df; XMStoreFloat3(&df, d);
		_camYaw = atan2f(df.x, df.z); _camPitch = asinf(df.y);
	}

	// ── 상수버퍼 갱신 (카메라 뷰 + 빛 방향 애니메이션 → RT 그림자 이동) ──
	XMMATRIX model = XMMatrixIdentity();
	XMVECTOR eye = XMLoadFloat3(&_camPos);
	XMVECTOR fwd = CamForward(_camYaw, _camPitch);
	XMVECTOR up  = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMMATRIX view = XMMatrixLookAtLH(eye, XMVectorAdd(eye, fwd), up);
	float aspect = float(_sceneW) / float(_sceneH); // 씬 RT 비율 (왜곡 방지)
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(_fov), aspect, _camNear, _camFar); // V7
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

	SceneCB cb;
	XMStoreFloat4x4(&cb.mvp, model * view * proj); // row_major HLSL → 전치 불필요
	XMStoreFloat4x4(&cb.model, model);
	cb.lightDir = XMFLOAT4(cosf(a) * 0.6f, -1.0f, sinf(a) * 0.6f, _lightIntensity); // w=세기
	XMStoreFloat4(&cb.camPos, eye);
	cb.gridMin  = XMFLOAT4(-6.5f, 0.2f, -6.5f, 0.f);
	cb.gridMax  = XMFLOAT4( 6.5f, 4.0f,  6.5f, 0.f);
	cb.gridDim  = XMFLOAT4(float(PROBE_X), float(PROBE_Y), float(PROBE_Z), 128.f); // 128 rays/probe
	cb.giParams = XMFLOAT4(_giStrength, _time * 60.f, _ambient, 0.f); // GI세기 / frame / 앰비언트
	XMStoreFloat4x4(&cb.invVP, XMMatrixInverse(nullptr, view * proj)); // 스카이 레이 복원
	cb.pointPos   = XMFLOAT4(_pointPos.x, _pointPos.y, _pointPos.z, _pointRadius);
	_flickerV = _flicker ? (0.65f + 0.35f * sinf(_time * 31.0f) * sinf(_time * 7.3f) + 0.1f * sinf(_time * 53.0f)) : 1.0f; // W9
	float pInt = _pointIntensity * _flickerV;
	cb.pointColor = XMFLOAT4(_pointColor.x * pInt, _pointColor.y * pInt, _pointColor.z * pInt, _pointOn ? 1.f : 0.f);
	cb.matParams  = XMFLOAT4(_matMetallic, _matRoughness, _matEmissive, _matTint);
	cb.sunColor   = XMFLOAT4(_sunColor.x, _sunColor.y, _sunColor.z, _envIntensity);
	cb.fog        = XMFLOAT4(_fogColor.x, _fogColor.y, _fogColor.z, _fogDensity);
	cb.grade      = XMFLOAT4(_contrast, _saturation, _temperature, _vignette);
	cb.skyZenith  = XMFLOAT4(_skyZenith.x, _skyZenith.y, _skyZenith.z, _shadowSoft);
	cb.skyHorizon = XMFLOAT4(_skyHorizon.x, _skyHorizon.y, _skyHorizon.z, _sunSize);
	cb.dbg        = XMFLOAT4(float(_debugView), _probeViz ? 1.f : 0.f, float(_tonemapOp), _reflectOn ? _reflectStrength : 0.f);
	// 스팟 라이트
	XMVECTOR sd = XMVector3Normalize(XMLoadFloat3(&_spotDir));
	XMFLOAT3 sdn; XMStoreFloat3(&sdn, sd);
	cb.spotPos    = XMFLOAT4(_spotPos.x, _spotPos.y, _spotPos.z, _spotRadius);
	cb.spotDir    = XMFLOAT4(sdn.x, sdn.y, sdn.z, cosf(XMConvertToRadians(_spotConeDeg)));
	cb.spotColor  = XMFLOAT4(_spotColor.x * _spotIntensity, _spotColor.y * _spotIntensity, _spotColor.z * _spotIntensity, _spotOn ? 1.f : 0.f);
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
	// 다중 점광원 (slot0 = 기본 점광원과 동기화)
	_ptPosArr[0] = cb.pointPos; _ptColArr[0] = cb.pointColor;
	for (int i = 0; i < MAX_PT; ++i) { cb.ptPos[i] = (i < _ptCount) ? _ptPosArr[i] : XMFLOAT4(0,0,0,0); cb.ptCol[i] = (i < _ptCount) ? _ptColArr[i] : XMFLOAT4(0,0,0,0); }
	memcpy(_cbMapped[_frameIndex], &cb, sizeof(cb));

	// ── 모델 갱신: 스키닝(or 바인드) + 기즈모 트랜스폼 적용 → VB (GPU 유휴, 전체 플러시) ──
	UpdateAnimation();

	auto alloc = _allocators[_frameIndex];
	ThrowIfFailed(alloc->Reset(), "Allocator Reset");
	ThrowIfFailed(_cmdList->Reset(alloc.Get(), nullptr), "CmdList Reset");

	// 갱신된 정점으로 BLAS/TLAS 매 프레임 재빌드 (스키닝/기즈모 반영 → RT 일치)
	RecordBuildAS();

	// ── DDGI: 프로브 irradiance 갱신 (컴퓨트 RT) — 라스터 전에 ──
	DispatchGI();

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

	// ── 스카이박스 (배경, 깊이 없음) — 솔리드 배경 모드면 생략 ──
	if (_showSky && _bgMode == 0)
	{
		_cmdList->SetPipelineState(_skyPSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->DrawInstanced(3, 1, 0, 0);
	}

	// ── 선택 아웃라인 (모델 선택 시, 인버티드 헐 → 모델이 위에 덮어 림만 남음) ──
	if (_sel == SelEntity::Model && _modelIndexCount > 0)
	{
		_cmdList->SetPipelineState(_outlinePSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &_vbv);
		_cmdList->IASetIndexBuffer(&_ibv);
		_cmdList->DrawIndexedInstanced(_modelIndexCount, 1, 0, 0, 0);
	}

	_cmdList->SetPipelineState(_wireframe ? _wirePSO.Get() : _pso.Get()); // 그래픽스 PSO (컴퓨트 후 복귀)
	_cmdList->SetGraphicsRootSignature(_rootSig.Get());
	_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
	_cmdList->SetGraphicsRootShaderResourceView(1, _tlas->GetGPUVirtualAddress()); // TLAS (RayQuery)
	_cmdList->SetGraphicsRootShaderResourceView(2, _probes->GetGPUVirtualAddress()); // DDGI 프로브
	_cmdList->SetGraphicsRootShaderResourceView(5, _probeDepth->GetGPUVirtualAddress()); // 프로브 depth
	if (_hasTexture)
	{
		ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, heaps);
	}
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, 1, &_vbv);
	_cmdList->IASetIndexBuffer(&_ibv);

	// 모델: 서브메시별 머티리얼 텍스처 테이블 오프셋 후 드로우
	if (_hasTexture && !_submeshes.empty())
	{
		_cmdList->SetGraphicsRoot32BitConstant(4, 1u, 0); // useTex
		D3D12_GPU_DESCRIPTOR_HANDLE base = _srvHeap->GetGPUDescriptorHandleForHeapStart();
		for (size_t i = 0; i < _submeshes.size(); ++i)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE h = base;
			h.ptr += SIZE_T(_subMatSlot[i]) * 3 * _srvInc; // 슬롯×3 디스크립터
			_cmdList->SetGraphicsRootDescriptorTable(3, h);
			_cmdList->DrawIndexedInstanced(_submeshes[i].indexCount, 1, _submeshes[i].indexStart, 0, 0);
		}
	}
	else
	{
		_cmdList->SetGraphicsRoot32BitConstant(4, 0u, 0);
		_cmdList->DrawIndexedInstanced(_modelIndexCount, 1, 0, 0, 0); // 모델(정점색 폴백)
	}

	// 바닥(정점색)
	_cmdList->SetGraphicsRoot32BitConstant(4, 0u, 0);
	_cmdList->DrawIndexedInstanced(_indexCount - _modelIndexCount, 1, _modelIndexCount, 0, 0);

	// ── 씬 그리드 (지오메트리 위, 깊이 테스트) ──
	if (_showGrid)
	{
		_cmdList->SetPipelineState(_gridPSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->DrawInstanced(3, 1, 0, 0);
	}

	// ── 디버그 라인 (본/AABB/콘/아이콘) ──
	DrawDebugLines();

	// ── DDGI 프로브 시각화 (점) ──
	if (_probeViz)
	{
		_cmdList->SetPipelineState(_probePSO.Get());
		_cmdList->SetGraphicsRootSignature(_rootSig.Get());
		_cmdList->SetGraphicsRootConstantBufferView(0, _cb[_frameIndex]->GetGPUVirtualAddress());
		_cmdList->SetGraphicsRootShaderResourceView(2, _probes->GetGPUVirtualAddress());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		_cmdList->DrawInstanced(PROBE_COUNT, 1, 0, 0);
	}

	// ── 블룸 (브라이트패스 → BlurH → BlurV, 반해상도) ──
	Transition(_sceneRT.Get(), _sceneRTState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (_bloomOn && _bloomReady)
	{
		UINT rInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE bA = _bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CPU_DESCRIPTOR_HANDLE bB = bA; bB.ptr += rInc;
		D3D12_GPU_DESCRIPTOR_HANDLE post = _postSrvHeap->GetGPUDescriptorHandleForHeapStart();
		ID3D12DescriptorHeap* ph[] = { _postSrvHeap.Get() };
		D3D12_VIEWPORT bvp{ 0,0,float(_bloomW),float(_bloomH),0,1 };
		D3D12_RECT bsc{ 0,0,LONG(_bloomW),LONG(_bloomH) };
		float tx = 1.0f / float(_bloomW), ty = 1.0f / float(_bloomH);

		auto pass = [&](ID3D12PipelineState* pso, D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT srcSlot, const float c[8])
		{
			_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			_cmdList->RSSetViewports(1, &bvp); _cmdList->RSSetScissorRects(1, &bsc);
			_cmdList->SetPipelineState(pso);
			_cmdList->SetGraphicsRootSignature(_postRootSig.Get());
			_cmdList->SetDescriptorHeaps(1, ph);
			D3D12_GPU_DESCRIPTOR_HANDLE t = post; t.ptr += UINT64(srcSlot) * _postSrvInc;
			_cmdList->SetGraphicsRootDescriptorTable(0, t);
			_cmdList->SetGraphicsRoot32BitConstants(1, 8, c, 0);
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->DrawInstanced(3, 1, 0, 0);
		};
		float bright[8] = { _bloomThreshold, 0,0,0,0,0,0,0 };
		float blurH[8] = { tx, ty, 1.0f, 0.0f, 0,0,0,0 };
		float blurV[8] = { tx, ty, 0.0f, 1.0f, 0,0,0,0 };
		// 브라이트패스: 씬(slot0) → bloomA
		Transition(_bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pass(_brightPSO.Get(), bA, 0, bright);
		Transition(_bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// BlurH: bloomA(slot1) → bloomB
		Transition(_bloomB.Get(), _bloomBState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pass(_blurPSO.Get(), bB, 1, blurH);
		Transition(_bloomB.Get(), _bloomBState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// BlurV: bloomB(slot3) → bloomA (최종)
		Transition(_bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pass(_blurPSO.Get(), bA, 3, blurV);
		Transition(_bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// ── 톤맵 (HDR 씬 RT → LDR RT, ACES + 노출 + 감마 + 블룸 + DOF/갓레이) ──
	Transition(_sceneDepth.Get(), _sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // DOF 입력
	Transition(_sceneLDR.Get(), _sceneLDRState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE ldrRtv = _sceneRtvHeap->GetCPUDescriptorHandleForHeapStart();
		ldrRtv.ptr += rtvInc; // slot1 = LDR
		_cmdList->OMSetRenderTargets(1, &ldrRtv, FALSE, nullptr);
		_cmdList->RSSetViewports(1, &vp);
		_cmdList->RSSetScissorRects(1, &sc);
		_cmdList->SetPipelineState(_tonemapPSO.Get());
		_cmdList->SetGraphicsRootSignature(_postRootSig.Get());
		ID3D12DescriptorHeap* ph[] = { _postSrvHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, ph);
		_cmdList->SetGraphicsRootDescriptorTable(0, _postSrvHeap->GetGPUDescriptorHandleForHeapStart());
		float pc[8] = { _exposure * powf(2.0f, _ev), _bloomIntensity, (_bloomOn && _bloomReady) ? 1.0f : 0.0f, float(_tonemapOp),
		                _contrast, _saturation, _temperature, _vignette }; // V13 EV
		_cmdList->SetGraphicsRoot32BitConstants(1, 8, pc, 0);
		float pc2[8] = { _chroma, _grain, _sharpen, _time * 60.0f, 1.0f / float(_sceneW), 1.0f / float(_sceneH), _expScale, 0.0f };
		_cmdList->SetGraphicsRoot32BitConstants(2, 8, pc2, 0);
		// 태양 화면 위치(갓레이) + DOF 파라미터 (b2)
		XMVECTOR sunW = XMVectorAdd(eye, XMVectorScale(fwd, 0.0f)); // placeholder
		sunW = XMVectorAdd(eye, XMVectorScale(XMVector3Normalize(XMVectorNegate(XMLoadFloat4(&cb.lightDir))), 500.0f));
		XMVECTOR sc4 = XMVector4Transform(XMVectorSetW(sunW, 1.0f), view * proj);
		float sw = XMVectorGetW(sc4); float sunSX = 0.5f, sunSY = 0.5f; bool sunVis = sw > 0.0f;
		if (sunVis) { sunSX = XMVectorGetX(sc4) / sw * 0.5f + 0.5f; sunSY = -XMVectorGetY(sc4) / sw * 0.5f + 0.5f; }
		float pc3[8] = { sunSX, sunSY, _volStrength, _dofFocus, _dofRange, _dofOn ? 1.0f : 0.0f, (_volOn && sunVis) ? 1.0f : 0.0f, 0.0f };
		_cmdList->SetGraphicsRoot32BitConstants(3, 8, pc3, 0);
		D3D12_GPU_DESCRIPTOR_HANDLE depthH = _postSrvHeap->GetGPUDescriptorHandleForHeapStart(); depthH.ptr += UINT64(2) * _postSrvInc; // slot2 depth
		_cmdList->SetGraphicsRootDescriptorTable(4, depthH);
		float pc4[8] = { _lensDistort, _posterize, _anamorphic ? 1.0f : 0.0f, float(_filterMode), 0,0,0,0 };
		_cmdList->SetGraphicsRoot32BitConstants(5, 8, pc4, 0);
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->DrawInstanced(3, 1, 0, 0);
	}
	Transition(_sceneLDR.Get(), _sceneLDRState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// ── FXAA (LDR → LDR2), 표시 텍스처 선택 ──
	ID3D12Resource* displayRes = _sceneLDR.Get();
	if (_fxaaOn)
	{
		Transition(_sceneLDR2.Get(), _sceneLDR2State, D3D12_RESOURCE_STATE_RENDER_TARGET);
		UINT rInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv2 = _sceneRtvHeap->GetCPUDescriptorHandleForHeapStart(); rtv2.ptr += SIZE_T(rInc) * 2;
		_cmdList->OMSetRenderTargets(1, &rtv2, FALSE, nullptr);
		_cmdList->RSSetViewports(1, &vp); _cmdList->RSSetScissorRects(1, &sc);
		_cmdList->SetPipelineState(_fxaaPSO.Get());
		_cmdList->SetGraphicsRootSignature(_postRootSig.Get());
		ID3D12DescriptorHeap* ph2[] = { _postSrvHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, ph2);
		D3D12_GPU_DESCRIPTOR_HANDLE t = _postSrvHeap->GetGPUDescriptorHandleForHeapStart(); t.ptr += UINT64(4) * _postSrvInc; // slot4 = LDR
		_cmdList->SetGraphicsRootDescriptorTable(0, t);
		float fc[8] = { 1.0f / float(_sceneW), 1.0f / float(_sceneH), 0,0,0,0,0,0 };
		_cmdList->SetGraphicsRoot32BitConstants(1, 8, fc, 0);
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->DrawInstanced(3, 1, 0, 0);
		Transition(_sceneLDR2.Get(), _sceneLDR2State, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		displayRes = _sceneLDR2.Get();
	}
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

	ThrowIfFailed(_swapChain->Present(1, 0), "Present");
	MoveToNextFrame();

	if (_wantShot) { _wantShot = false; SaveScreenshot(); if (_hiresShot) { _renderScale = 1.0f; _hiresShot = false; } } // GPU 유휴 시점 리드백
}
