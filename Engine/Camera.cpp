#include "pch.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "BindShaderDesc.h"
#include "Light.h"
#include "Terrain.h"
#include "Mesh.h"
#include "Model.h"
#include "ModelMesh.h"
#include "Ibl.h"
#include "GBuffer.h"
#include "HlslShader.h"
#include "RenderStateManager.h"

shared_ptr<LightArrayDesc> Camera::CollectLights(shared_ptr<Scene> scene)
{
	auto lightArray = make_shared<LightArrayDesc>();
	lightArray->lightCount = 0;

	auto& lights = scene->GetLights();
	for (auto& lightObj : lights)
	{
		if (lightArray->lightCount >= MAX_LIGHTS) break;
		auto lc = lightObj->GetLight();
		if (!lc) continue;

		const LightDesc& ld = lc->GetLightDesc();
		LightData& data = lightArray->lights[lightArray->lightCount];

		data.diffuse    = ld.diffuse;
		data.ambient    = ld.ambient;
		data.intensity  = ld.intensity;
		data.type       = static_cast<int32>(lc->GetLightType());
		data.direction  = ld.direction;
		data.position   = lightObj->GetTransform()->GetPosition();
		data.range      = lc->GetRange();
		data.attenuation = lc->GetAttenuation();
		data.spotAngle  = lc->GetSpotAngleCos();
		// 점/스팟 그림자 슬롯 (PunctualShadowMap::Draw 가 같은 프레임 pre-render 에서 할당)
		// 스팟=2D 배열 슬롯, 포인트=큐브 배열 슬롯 (셰이더에서 type 으로 분기)
		data.shadowIndex = (lc->GetLightType() == LightType::Spot || lc->GetLightType() == LightType::Point)
			? lc->GetShadowSlot() : -1;

		lightArray->lightCount++;
	}
	return lightArray;
}

void Camera::SortGameObject()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	_vecForward.clear();
	_vecOpaque.clear();
	_vecBackground.clear();
	_vecTransparent.clear();

	// 매 프레임 절두체 갱신
	_frustum.Update(_matView * _matProjection);

	Vec3 camPos = GetTransform()->GetPosition();

	for (auto& gameObject : gameObjects)
	{
		if (IsCulled(gameObject->GetLayerIndex()))
			continue;

		// 에디터 내부 오브젝트(씬 그리드, 드래그 프리뷰)는 에디터 카메라 시점에서만 렌더
		if (gameObject->IsEditorInternal() && GetGameObject()->IsEditorInternal() == false)
			continue;

		// Renderer 슬롯 공용 getter — Mesh/Model/Animator 외 커스텀 렌더러(Particle 등)도 큐에 태운다
		auto renderer = gameObject->GetRenderer();

		if (renderer == nullptr)
			continue;

		// ✅ 핵심 수정: 절두체 컬링 전에 BoundingBox를 현재 월드 위치로 갱신
		renderer->TransformBoundingBox();  // ← 월드 행렬 기준으로 BoundingBox 갱신

		// RenderQueue 분류 — 커스텀 렌더러는 자체 큐, MeshRenderer 는 머티리얼 큐 우선
		RenderQueue queue = renderer->GetRenderQueue();
		if (auto mr = gameObject->GetMeshRenderer())
		{
			if (mr->GetMaterial()) queue = mr->GetMaterial()->GetRenderQueue();
		}
		else if (renderer->GetRenderType() == RendererType::Particle)
		{
			queue = RenderQueue::Transparent; // 파티클은 항상 투명 패스 (Pass 3, HDR sceneColor)
		}

		// Frustum Culling — Background(스카이박스)는 제외:
		// 스카이 VS 는 w=0 트릭으로 항상 풀스크린을 그리지만 메시 AABB 는 원점의 작은 구라서
		// 카메라가 원점을 안 보면 컬링돼 하늘이 통째로 사라지는 버그가 있었음
		if (queue != RenderQueue::Background)
		{
			const BoundingBox& box = renderer->GetBoundingBox();
			bool hasValidBox = (box.Extents.x > 0.f || box.Extents.y > 0.f || box.Extents.z > 0.f);
			if (hasValidBox && !_frustum.IsInFrustum(box))
				continue;
		}

		if (static_cast<int32>(queue) >= static_cast<int32>(RenderQueue::Transparent))
			_vecTransparent.push_back(gameObject);
		else if (queue == RenderQueue::Background)
			_vecBackground.push_back(gameObject);
		else
			_vecOpaque.push_back(gameObject);

		_vecForward.push_back(gameObject);
	}

	// 불투명: Front-to-Back 정렬
	sort(_vecOpaque.begin(), _vecOpaque.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da < db;
		});

	// 투명: Back-to-Front 정렬
	sort(_vecTransparent.begin(), _vecTransparent.end(),
		[&camPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
		{
			float da = Vec3::DistanceSquared(a->GetTransform()->GetPosition(), camPos);
			float db = Vec3::DistanceSquared(b->GetTransform()->GetPosition(), camPos);
			return da > db;
		});
}

void Camera::Render_Forward()
{
	auto cam   = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	auto scene = SCENE->GetCurrentScene();
	
	int32 tech = 0;
	if (INPUT->GetButton(KEY_TYPE::KEY_1)) tech = TECH_WIREFRAME;
	if (INPUT->GetButton(KEY_TYPE::KEY_2)) tech = TECH_CLOCKWISE;

	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();

	auto lightArray = CollectLights(scene);

	// RenderContext 구성
	RenderContext baseCtx;
	baseCtx.tech = tech;
	baseCtx.view   = V;
	baseCtx.proj   = P;
	baseCtx.light  = nullptr;
	baseCtx.lightArray = lightArray;
	baseCtx.hlslOverride   = nullptr;
	baseCtx.buffer = nullptr;

	// 1) 불투명 렌더 패스 (Front-to-Back, Depth Write ON)
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecOpaque);

	// 2) Background 렌더 패스 (스카이박스: 모든 불투명 지오메트리 이후, 투명 이전)
	//    SkyBoxDepth DSS (LESS_EQUAL + 깊이 쓰기 없음)로 빈 배경만 채움
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecBackground);

	// 3) 투명 렌더 패스 (Back-to-Front)
	GET_SINGLE(InstancingManager)->Render(baseCtx, _vecTransparent);
}

void Camera::Render_Deferred()
{
	auto scene = SCENE->GetCurrentScene();

	// 자기 자신의 시점으로 렌더 — 메인(에디터) 카메라뿐 아니라 게임 카메라(Game 뷰)도 호출 가능
	Matrix V = GetViewMatrix();
	Matrix P = GetProjectionMatrix();

	auto lightArray = CollectLights(scene);

	// G-Buffer lazy init — 실제 씬 뷰포트 크기에 맞춰 생성 (씬 윈도우 리사이즈 대응)
	// 카메라 _width/_height 가 갱신되지 않아 GBuffer가 고정 크기로 만들어지던 버그 수정
	uint32 w = static_cast<uint32>(GRAPHICS->GetViewport().GetWidth());
	uint32 h = static_cast<uint32>(GRAPHICS->GetViewport().GetHeight());
	if (w == 0 || h == 0)
		return;

	_width  = static_cast<float>(w);
	_height = static_cast<float>(h);

	if (!_gBuffer || _gBuffer->GetWidth() != w || _gBuffer->GetHeight() != h)
	{
		_gBuffer = make_shared<GBuffer>();
		_gBuffer->Init(w, h);
		EnsureSceneColor(w, h);
	}

	// ── Pass 1: G-Buffer fill (opaque only) ──
	_gBuffer->Clear();
	_gBuffer->BindAsTarget();

	// 외부 패스(인스펙터 프리뷰 FX 렌더 등)가 남긴 렌더 상태에 영향받지 않도록
	// 불투명 GBuffer fill 상태를 명시적으로 강제한다.
	DCT->OMSetBlendState(RENDER_STATES->GetBS(BlendStateType::Default).Get(), nullptr, 0xFFFFFFFF);
	DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::Default).Get(), 0);
	DCT->RSSetState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullBack).Get());

	// 씬 뷰 와이어프레임 토글 — GBuffer 지오메트리 패스 동안만 (풀스크린 라이팅 패스는 솔리드 유지)
	HlslShader::S_ForceWireframe = _wireframe;

	RenderContext gbufCtx;
	gbufCtx.view = V;
	gbufCtx.proj = P;
	gbufCtx.lightArray = lightArray;
	gbufCtx.deferredPass = true;
	GET_SINGLE(InstancingManager)->Render(gbufCtx, _vecOpaque);

	// 터레인도 GBuffer 로 (포워드 특수경로 제거 — 디퍼드 라이팅/PBR/SSAO 일괄 적용)
	if (auto terrainObj = scene->GetTerrain())
	{
		if (auto terrain = terrainObj->GetTerrain())
		{
			terrain->TerrainRendererGBuffer(V, P);
			// 식생 — 터레인 직후 GBuffer 로 (바람 애니: 프레임 델타 누적)
			float dt = GET_SINGLE(TimeManager)->GetDeltaTime();
			terrain->RenderFoliageGBuffer(V, P, dt); // 잔디
			terrain->RenderTreesGBuffer(V, P, dt);   // 나무
		}
	}

	HlslShader::S_ForceWireframe = false; // 이후 패스(라이팅/포스트)는 항상 솔리드

	// ── Pass 2: Deferred lighting (fullscreen) → HDR sceneColor ──
	// 백버퍼가 아닌 씬 크기 HDR 버퍼에 라이팅 — GBuffer DSV 와 크기가 일치하므로
	// Pass 3 의 깊이 테스트(스카이 z=far, 투명체)가 올바르게 동작한다.
	{
		float clearBlack[4] = { 0, 0, 0, 0 };
		DCT->ClearRenderTargetView(_sceneColorRTV.Get(), clearBlack);

		ID3D11RenderTargetView* hdrRTV = _sceneColorRTV.Get();
		DCT->OMSetRenderTargets(1, &hdrRTV, _gBuffer->GetDSV().Get());

		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(w);
		vp.Height = static_cast<float>(h);
		vp.MaxDepth = 1.f;
		DCT->RSSetViewports(1, &vp);
	}

	auto lightingShader = RESOURCES->Get<HlslShader>(L"DeferredLighting_HLSL");
	if (lightingShader)
	{
		RENDER_STATES->BindAllSamplersPS();
		_gBuffer->BindSRVsPS(0);

		// Emissive RT — t3(Shadow)/t4(Ssao)/t5~7(IBL) 다음의 t8
		lightingShader->SetPSSRV(8, _gBuffer->GetSRV(GBuffer::RT_EMISSIVE).Get());

		bool hasSsao = false;
		if (auto mat = RESOURCES->Get<Material>(L"DefaultMaterial"))
		{
			if (mat->GetShadowMap())
				lightingShader->SetPSSRV(3, mat->GetShadowMap()->GetComPtr().Get());
			if (mat->GetSsaoMap())
			{
				lightingShader->SetPSSRV(4, mat->GetSsaoMap().Get());
				hasSsao = true;
			}
		}

		// IBL (t5~t7 + b8)
		if (Ibl::IsReady())
		{
			lightingShader->SetPSSRV(5, Ibl::GetIrradiance().Get());
			lightingShader->SetPSSRV(6, Ibl::GetPrefiltered().Get());
			lightingShader->SetPSSRV(7, Ibl::GetBrdfLut().Get());
		}
		if (_iblCB == nullptr)
		{
			_iblCB = make_shared<ConstantBuffer<IblDesc>>();
			_iblCB->Create();
		}
		IblDesc iblDesc;
		iblDesc.useIbl = Ibl::IsReady() ? 1 : 0;
		iblDesc.envIntensity = _envIntensity;
		_iblCB->CopyData(iblDesc);
		lightingShader->SetPSConstantBuffer(8, _iblCB->GetComPtr().Get());

		// CSM (b9) — 캐스케이드 행렬/스플릿 (Light::UpdateCascades 가 메인 카메라 기준 매 프레임 갱신)
		if (_cascadeCB == nullptr)
		{
			_cascadeCB = make_shared<ConstantBuffer<CascadeDesc>>();
			_cascadeCB->Create();
		}
		CascadeDesc csm;
		for (int32 c = 0; c < CASCADE_COUNT; ++c)
			csm.cascadeVPT[c] = Light::S_CascadeVPT[c];
		csm.cascadeSplits = Vec4(Light::S_CascadeSplitView[0], Light::S_CascadeSplitView[1],
		                         Light::S_CascadeSplitView[2], Light::S_CascadeSplitView[3]);
		csm.cascadeCount = CASCADE_COUNT;
		csm.cascadeDebug = _csmDebug ? 1 : 0;
		_cascadeCB->CopyData(csm);
		lightingShader->SetPSConstantBuffer(9, _cascadeCB->GetComPtr().Get());

		// 점/스팟 그림자 — 스팟 섀도우 배열(t9) + 스팟 V*P*T(b10)
		if (auto mat = RESOURCES->Get<Material>(L"DefaultMaterial"))
		{
			if (mat->GetSpotShadowMap())
				lightingShader->SetPSSRV(9, mat->GetSpotShadowMap()->GetComPtr().Get());
			if (mat->GetPointShadowCube())
				lightingShader->SetPSSRV(10, mat->GetPointShadowCube().Get());
		}
		if (_punctualCB == nullptr)
		{
			_punctualCB = make_shared<ConstantBuffer<PunctualShadowDesc>>();
			_punctualCB->Create();
		}
		PunctualShadowDesc punctual;
		for (int32 s = 0; s < MAX_PUNCTUAL_SHADOWS; ++s)
			punctual.spotVPT[s] = Light::S_SpotVPT[s];
		_punctualCB->CopyData(punctual);
		lightingShader->SetPSConstantBuffer(10, _punctualCB->GetComPtr().Get());

		lightingShader->Bind();
		lightingShader->PushGlobalData(V, P);
		lightingShader->PushLightArrayData(*lightArray);

		MaterialDesc defaultMat;
		defaultMat.ambient  = Vec4(1.f);
		defaultMat.diffuse  = Vec4(1.f);
		defaultMat.specular = Vec4(1.f, 1.f, 1.f, 32.f);
		defaultMat.useSsao  = hasSsao ? 1 : 0;
		lightingShader->PushMaterialData(defaultMat);

		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
		DCT->Draw(3, 0);
		DCT->OMSetDepthStencilState(nullptr, 0);

		_gBuffer->UnbindSRVsPS(0);
		lightingShader->SetPSSRV(5, nullptr);
		lightingShader->SetPSSRV(6, nullptr);
		lightingShader->SetPSSRV(7, nullptr);
		lightingShader->SetPSSRV(8, nullptr); // Emissive RT 해제 (다음 프레임 GBuffer RTV 바인딩과 충돌 방지)
	}

	// ── Pass 2.5: SSR (스크린스페이스 반사) — sceneColor(불투명 라이팅 결과) + GBuffer ──
	if (_ssrEnabled)
		RenderSSR(V, P, w, h);

	// ── Pass 3: Forward pass (skybox + transparent) — sceneColor + G-Buffer depth ──
	RenderContext fwdCtx;
	fwdCtx.view = V;
	fwdCtx.proj = P;
	fwdCtx.lightArray = lightArray;
	GET_SINGLE(InstancingManager)->Render(fwdCtx, _vecBackground);
	GET_SINGLE(InstancingManager)->Render(fwdCtx, _vecTransparent);

	// ── 에디터 전용: 선택 오브젝트 아웃라인 (씬 뷰 카메라에서만, sceneColor + GBuffer 스텐실) ──
	if (GetGameObject()->IsEditorInternal())
		RenderOutlinePass(V, P);

	// ── Bloom: sceneColor → 하프 해상도 brightpass + 가우시안 블러 ──
	if (_bloomEnabled)
		RenderBloom(w, h);

	// 최종 출력 바인딩 — 기본은 백버퍼, _finalRTV 지정 시 외부 RT (Game 뷰)
	auto bindFinalTarget = [&]()
	{
		if (_finalRTV)
		{
			ID3D11RenderTargetView* rtv = _finalRTV.Get();
			DCT->OMSetRenderTargets(1, &rtv, nullptr);

			D3D11_VIEWPORT vp{};
			vp.Width = static_cast<float>(w);
			vp.Height = static_cast<float>(h);
			vp.MaxDepth = 1.f;
			DCT->RSSetViewports(1, &vp);
		}
		else
		{
			GRAPHICS->RestoreMainRenderTarget();
		}
	};

	// ── Pass 4: Tonemap — HDR sceneColor (+Bloom) → 최종 타겟 또는 LDR 버퍼 (ACES + 감마) ──
	// FXAA 켜져 있으면 LDR 중간 버퍼에 톤매핑하고 FXAA 가 최종 타겟으로 마무리
	if (_fxaaEnabled)
	{
		ID3D11RenderTargetView* ldrRTV = _ldrRTV.Get();
		DCT->OMSetRenderTargets(1, &ldrRTV, nullptr);

		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(w);
		vp.Height = static_cast<float>(h);
		vp.MaxDepth = 1.f;
		DCT->RSSetViewports(1, &vp);
	}
	else
	{
		bindFinalTarget();
	}

	if (auto tonemapShader = RESOURCES->Get<HlslShader>(L"Tonemap_HLSL"))
	{
		RENDER_STATES->BindAllSamplersPS();
		tonemapShader->Bind();
		tonemapShader->SetPSSRV(0, _sceneColorSRV.Get());
		tonemapShader->SetPSSRV(1, _bloomEnabled ? _bloomSRV[0].Get() : nullptr);

		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
		DCT->Draw(3, 0);
		DCT->OMSetDepthStencilState(nullptr, 0);

		tonemapShader->SetPSSRV(0, nullptr);
		tonemapShader->SetPSSRV(1, nullptr);
	}

	// ── FXAA: LDR 중간 버퍼 → 최종 타겟 ──
	if (_fxaaEnabled)
	{
		bindFinalTarget();

		if (auto fxaaShader = RESOURCES->Get<HlslShader>(L"Fxaa_HLSL"))
		{
			if (_postCB == nullptr)
			{
				_postCB = make_shared<ConstantBuffer<PostProcessDesc>>();
				_postCB->Create();
			}
			PostProcessDesc pd;
			pd.texelSize = Vec2(1.f / w, 1.f / h);
			_postCB->CopyData(pd);

			RENDER_STATES->BindAllSamplersPS();
			fxaaShader->Bind();
			fxaaShader->SetPSSRV(0, _ldrSRV.Get());
			fxaaShader->SetPSConstantBuffer(8, _postCB->GetComPtr().Get());

			DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
			DCT->Draw(3, 0);
			DCT->OMSetDepthStencilState(nullptr, 0);

			fxaaShader->SetPSSRV(0, nullptr);
		}
	}

	// ── 씬뷰 패스 뷰어 (에디터 콤보 또는 KEY_4 순환, 0=Final 이면 스킵) ──
	if (INPUT->GetButtonDown(KEY_TYPE::KEY_4))
		_debugViewMode = (_debugViewMode + 1) % 9;

	if (_debugViewMode != 0)
		RenderPassViewer(V, P);

	// ── G-Buffer Debug View (KEY_3 toggle) ──
	if (INPUT->GetButtonDown(KEY_TYPE::KEY_3))
		_showGBufferDebug = !_showGBufferDebug;

	if (_showGBufferDebug)
	{
		auto debugShader = RESOURCES->Get<HlslShader>(L"GBufferDebug_HLSL");
		if (debugShader)
		{
			RENDER_STATES->BindAllSamplersPS();
			_gBuffer->BindSRVsPS(0);

			debugShader->Bind();
			debugShader->PushGlobalData(V, P);

			DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
			DCT->Draw(3, 0);
			DCT->OMSetDepthStencilState(nullptr, 0);

			_gBuffer->UnbindSRVsPS(0);
		}
	}
}

// 에디터 선택 아웃라인 — 스텐실 2패스 (마크: 메시 영역을 스텐실에 기록, 드로우: 팽창 메시를 영역 밖에만)
// Pass 3 직후 호출 전제: _sceneColorRTV + GBuffer DSV(D24S8) 바인딩 상태
void Camera::RenderOutlinePass(const Matrix& V, const Matrix& P)
{
	auto meshShader  = RESOURCES->Get<HlslShader>(L"Outline_HLSL");
	auto modelShader = RESOURCES->Get<HlslShader>(L"OutlineModel_HLSL");
	auto animShader  = RESOURCES->Get<HlslShader>(L"OutlineAnim_HLSL");
	if (!meshShader || !modelShader || !animShader)
		return;

	if (_outlineCB == nullptr)
	{
		_outlineCB = make_shared<ConstantBuffer<OutlineDesc>>();
		_outlineCB->Create();
	}

	HlslShader* shaders[] = { meshShader.get(), modelShader.get(), animShader.get() };

	// 마크/드로우 패스 상태 — 컬링 없음 (Z 미러 변환 모델의 와인딩 안전)
	auto setPassStates = [&](bool markPass)
	{
		for (auto* s : shaders)
		{
			s->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullNone));
			if (markPass)
			{
				s->SetBlendState(RENDER_STATES->GetBS(BlendStateType::NoColorWrite));
				s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::OutlineMark), 1);
			}
			else
			{
				s->SetBlendState(RENDER_STATES->GetBS(BlendStateType::Default));
				s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::OutlineDraw), 1);
			}
		}
	};

	auto drawObject = [&](const shared_ptr<GameObject>& go, float width)
	{
		OutlineDesc desc;
		desc.width = width;
		_outlineCB->CopyData(desc);
		ID3D11Buffer* cb = _outlineCB->GetComPtr().Get();

		Matrix world = go->GetTransform()->GetWorldMatrix();

		if (auto mr = go->GetMeshRenderer())
		{
			if (mr->GetMesh() == nullptr)
				return;

			meshShader->SetVSConstantBuffer(8, cb);
			meshShader->SetPSConstantBuffer(8, cb);
			meshShader->PushGlobalData(V, P);
			meshShader->PushTransformData(TransformDesc{ world });

			mr->GetMesh()->GetVertexBuffer()->PushData();
			mr->GetMesh()->GetIndexBuffer()->PushData();
			meshShader->DrawIndexed(mr->GetMesh()->GetIndexBuffer()->GetCount(), 0, 0);
		}
		else if (auto mdr = go->GetModelRenderer())
		{
			auto model = mdr->GetModel();
			if (model == nullptr)
				return;

			modelShader->SetVSConstantBuffer(8, cb);
			modelShader->SetPSConstantBuffer(8, cb);
			modelShader->PushGlobalData(V, P);
			modelShader->PushTransformData(TransformDesc{ world });

			for (auto& mesh : model->GetMeshes())
			{
				modelShader->PushModelBoneData(model->GetBoneByIndex(mesh->boneIndex)->transform);
				mesh->vertexBuffer->PushData();
				mesh->indexBuffer->PushData();
				modelShader->DrawIndexed(mesh->indexBuffer->GetCount(), 0, 0);
			}
		}
		else if (auto ma = go->GetModelAnimator())
		{
			auto model = ma->GetModel();
			if (model == nullptr || ma->GetTransformMapSRV() == nullptr)
				return;

			animShader->SetVSConstantBuffer(8, cb);
			animShader->SetPSConstantBuffer(8, cb);
			animShader->PushGlobalData(V, P);
			animShader->PushTransformData(TransformDesc{ world });
			animShader->SetVSSRV(5, ma->GetTransformMapSRV().Get());

			// 단일 드로우 → SV_InstanceID = 0 슬롯에 현재 트윈 상태 push
			auto tween = make_shared<InstancedTweenDesc>();
			tween->tweens[0] = ma->GetTweenDesc();
			animShader->PushTweenData(*tween);

			for (auto& mesh : model->GetMeshes())
			{
				mesh->vertexBuffer->PushData();
				mesh->indexBuffer->PushData();
				animShader->DrawIndexed(mesh->indexBuffer->GetCount(), 0, 0);
			}
		}
	};

	Vec3 camPos = GetTransform()->GetPosition();

	for (auto& go : _vecForward)
	{
		if (go->GetUIPicked() == false || go->GetEnableOutline() == false)
			continue;
		if (go->GetRenderer() == nullptr)
			continue;

		// 거리 비례 폭 — 화면상 두께를 대략 유지하되, 멀어질수록 월드 팽창량이 무한정 커져
		// 둥근 부위(머리 등)가 부풀어 굵어지던 문제로 상한을 둔다.
		// 상한 이후엔 월드 고정폭 → 멀수록 화면상 얇아짐("멀리서 줄어들어야" 충족).
		float dist  = Vec3::Distance(camPos, go->GetTransform()->GetPosition());
		float width = dist * 0.0018f;
		width = max(0.006f, min(width, 0.04f));

		setPassStates(true);
		drawObject(go, 0.f);    // 마크: 원본 메시 영역을 스텐실에 기록
		setPassStates(false);
		drawObject(go, width);  // 드로우: 팽창 메시가 마크 영역 밖에만 그려져 외곽선이 된다
	}
}

// 씬뷰 패스 뷰어 — GBuffer/SSAO/Shadow 채널을 풀스크린으로 시각화 (톤매핑 결과 위에 덮어씀)
void Camera::RenderPassViewer(const Matrix& V, const Matrix& P)
{
	auto viewerShader = RESOURCES->Get<HlslShader>(L"PassViewer_HLSL");
	if (!viewerShader || !_gBuffer)
		return;

	RENDER_STATES->BindAllSamplersPS();
	_gBuffer->BindSRVsPS(0);

	if (auto mat = RESOURCES->Get<Material>(L"DefaultMaterial"))
	{
		if (mat->GetSsaoMap())
			viewerShader->SetPSSRV(3, mat->GetSsaoMap().Get());
		if (mat->GetShadowMap())
			viewerShader->SetPSSRV(4, mat->GetShadowMap()->GetComPtr().Get());
	}

	viewerShader->Bind();
	viewerShader->PushGlobalData(V, P);

	if (_passViewerCB == nullptr)
	{
		_passViewerCB = make_shared<ConstantBuffer<PassViewerDesc>>();
		_passViewerCB->Create();
	}
	PassViewerDesc pv;
	pv.viewMode = _debugViewMode;
	_passViewerCB->CopyData(pv);
	viewerShader->SetPSConstantBuffer(8, _passViewerCB->GetComPtr().Get());

	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
	DCT->Draw(3, 0);
	DCT->OMSetDepthStencilState(nullptr, 0);

	_gBuffer->UnbindSRVsPS(0);
	viewerShader->SetPSSRV(3, nullptr);
	viewerShader->SetPSSRV(4, nullptr);
}

// 씬 크기 HDR 컬러 버퍼 (재)생성 — GBuffer 재생성 시점에 함께 호출
void Camera::EnsureSceneColor(uint32 w, uint32 h)
{
	_sceneColorTex.Reset();
	_sceneColorRTV.Reset();
	_sceneColorSRV.Reset();

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	CHECK(DEVICE->CreateTexture2D(&desc, nullptr, _sceneColorTex.GetAddressOf()));
	CHECK(DEVICE->CreateRenderTargetView(_sceneColorTex.Get(), nullptr, _sceneColorRTV.GetAddressOf()));
	CHECK(DEVICE->CreateShaderResourceView(_sceneColorTex.Get(), nullptr, _sceneColorSRV.GetAddressOf()));

	// SSR 결과 버퍼 (sceneColor 와 동일 포맷 — 반사 합성 후 sceneColor 로 CopyResource)
	_ssrTex.Reset(); _ssrRTV.Reset(); _ssrSRV.Reset();
	CHECK(DEVICE->CreateTexture2D(&desc, nullptr, _ssrTex.GetAddressOf()));
	CHECK(DEVICE->CreateRenderTargetView(_ssrTex.Get(), nullptr, _ssrRTV.GetAddressOf()));
	CHECK(DEVICE->CreateShaderResourceView(_ssrTex.Get(), nullptr, _ssrSRV.GetAddressOf()));

	// Bloom ping-pong (하프 해상도 HDR)
	D3D11_TEXTURE2D_DESC bloomDesc = desc;
	bloomDesc.Width  = max(1u, w / 2);
	bloomDesc.Height = max(1u, h / 2);
	for (int i = 0; i < 2; ++i)
	{
		_bloomTex[i].Reset(); _bloomRTV[i].Reset(); _bloomSRV[i].Reset();
		CHECK(DEVICE->CreateTexture2D(&bloomDesc, nullptr, _bloomTex[i].GetAddressOf()));
		CHECK(DEVICE->CreateRenderTargetView(_bloomTex[i].Get(), nullptr, _bloomRTV[i].GetAddressOf()));
		CHECK(DEVICE->CreateShaderResourceView(_bloomTex[i].Get(), nullptr, _bloomSRV[i].GetAddressOf()));
	}

	// FXAA 입력용 LDR 중간 버퍼 (톤매핑 결과)
	D3D11_TEXTURE2D_DESC ldrDesc = desc;
	ldrDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	_ldrTex.Reset(); _ldrRTV.Reset(); _ldrSRV.Reset();
	CHECK(DEVICE->CreateTexture2D(&ldrDesc, nullptr, _ldrTex.GetAddressOf()));
	CHECK(DEVICE->CreateRenderTargetView(_ldrTex.Get(), nullptr, _ldrRTV.GetAddressOf()));
	CHECK(DEVICE->CreateShaderResourceView(_ldrTex.Get(), nullptr, _ldrSRV.GetAddressOf()));
}

// SSR — sceneColor(불투명 라이팅 결과) + GBuffer 로 반사 합성 → _ssrTex → sceneColor 로 복사.
// 별도 타겟에 (sceneColor + 반사) 를 쓰고 CopyResource 로 되돌려 read-write 충돌 회피.
void Camera::RenderSSR(const Matrix& V, const Matrix& P, uint32 w, uint32 h)
{
	auto shader = RESOURCES->Get<HlslShader>(L"Ssr_HLSL");
	if (shader == nullptr || _ssrRTV == nullptr || _sceneColorSRV == nullptr || _gBuffer == nullptr)
		return;

	ID3D11RenderTargetView* rtv = _ssrRTV.Get();
	DCT->OMSetRenderTargets(1, &rtv, nullptr);

	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(w);
	vp.Height = static_cast<float>(h);
	vp.MaxDepth = 1.f;
	DCT->RSSetViewports(1, &vp);

	RENDER_STATES->BindAllSamplersPS();
	shader->SetPSSRV(0, _sceneColorSRV.Get());
	shader->SetPSSRV(1, _gBuffer->GetSRV(GBuffer::RT_ALBEDO).Get());
	shader->SetPSSRV(2, _gBuffer->GetSRV(GBuffer::RT_NORMAL).Get());
	shader->SetPSSRV(3, _gBuffer->GetSRV(GBuffer::RT_POSITION).Get());

	shader->Bind();
	shader->PushGlobalData(V, P);

	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);
	DCT->Draw(3, 0);
	DCT->OMSetDepthStencilState(nullptr, 0);

	// SRV 해제 후 결과를 sceneColor 로 복사
	shader->SetPSSRV(0, nullptr);
	shader->SetPSSRV(1, nullptr);
	shader->SetPSSRV(2, nullptr);
	shader->SetPSSRV(3, nullptr);
	DCT->CopyResource(_sceneColorTex.Get(), _ssrTex.Get());
}

// Bloom — sceneColor 에서 임계값 이상 휘도 추출(하프 해상도) 후 분리형 가우시안 블러.
// 결과는 _bloomSRV[0], 톤매핑 패스가 t1 로 합성.
void Camera::RenderBloom(uint32 w, uint32 h)
{
	auto bright = RESOURCES->Get<HlslShader>(L"BloomBright_HLSL");
	auto blurH  = RESOURCES->Get<HlslShader>(L"BloomBlurH_HLSL");
	auto blurV  = RESOURCES->Get<HlslShader>(L"BloomBlurV_HLSL");
	if (!bright || !blurH || !blurV)
		return;

	if (_postCB == nullptr)
	{
		_postCB = make_shared<ConstantBuffer<PostProcessDesc>>();
		_postCB->Create();
	}

	uint32 bw = max(1u, w / 2);
	uint32 bh = max(1u, h / 2);

	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(bw);
	vp.Height = static_cast<float>(bh);
	vp.MaxDepth = 1.f;
	DCT->RSSetViewports(1, &vp);

	RENDER_STATES->BindAllSamplersPS();
	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DCT->OMSetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth).Get(), 0);

	auto fullscreenPass = [&](shared_ptr<HlslShader>& shader, ID3D11RenderTargetView* target,
		ID3D11ShaderResourceView* input, const Vec2& texelSize)
	{
		PostProcessDesc pd;
		pd.texelSize = texelSize;
		pd.bloomThreshold = _bloomThreshold;
		pd.bloomIntensity = _bloomIntensity;
		_postCB->CopyData(pd);

		DCT->OMSetRenderTargets(1, &target, nullptr);
		shader->Bind();
		shader->SetPSSRV(0, input);
		shader->SetPSConstantBuffer(8, _postCB->GetComPtr().Get());
		DCT->Draw(3, 0);
		shader->SetPSSRV(0, nullptr);
	};

	// BrightPass: sceneColor → bloom[0]
	fullscreenPass(bright, _bloomRTV[0].Get(), _sceneColorSRV.Get(), Vec2(1.f / w, 1.f / h));
	// BlurH: bloom[0] → bloom[1], BlurV: bloom[1] → bloom[0]
	fullscreenPass(blurH, _bloomRTV[1].Get(), _bloomSRV[0].Get(), Vec2(1.f / bw, 1.f / bh));
	fullscreenPass(blurV, _bloomRTV[0].Get(), _bloomSRV[1].Get(), Vec2(1.f / bw, 1.f / bh));

	DCT->OMSetDepthStencilState(nullptr, 0);
}

Camera::Camera() : Super(ComponentType::Camera)
{
	SceneWindowDesc sceneDesc;
	_width = static_cast<float>(sceneDesc.size.x);
	_height = static_cast<float>(sceneDesc.size.y);
}

Camera::~Camera()
{

}

void Camera::Update()
{
	UpdateMatrix();
}

void Camera::UpdateMatrix()
{
	Vec3 eyePosition = GetTransform()->GetPosition();
	Vec3 focusPosition = eyePosition + GetTransform()->GetLook();
	Vec3 upDirection = GetTransform()->GetUp();
	_matView = ::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

	if (_type == ProjectionType::Perspective)
	{
		_matProjection = ::XMMatrixPerspectiveFovLH(_fov, _width / _height, _near, _far);
	}
	else
	{
		_matProjection = ::XMMatrixOrthographicLH(_width, _height, _near, _far);
	}
}
