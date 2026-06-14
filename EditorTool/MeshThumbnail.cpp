#include "pch.h"
#include "MeshThumbnail.h"
#include "GameObject.h"
#include "Model.h"
#include "ModelMesh.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "SceneGrid.h"

shared_ptr<GameObject> MeshThumbnail::_gridObj = nullptr;
shared_ptr<SceneGrid>  MeshThumbnail::_grid    = nullptr;


MeshThumbnail::MeshThumbnail(uint32 width, uint32 height)
	: _width(width), _height(height)
{
	// 자기 RT 기준 원점 뷰포트 — 씬 창 offset(SceneDesc.x/y)을 넣으면 렌더 내용이
	// 그만큼 밀려 기록되어 썸네일이 중앙에서 벗어남
	_vp.Set(static_cast<float>(width), static_cast<float>(height));

	CreateColorTexture();
	CreateDepthStencilTexture();
}

void MeshThumbnail::ComputeFitViewProj(shared_ptr<GameObject> obj, float aspect, Matrix& outV, Matrix& outP)
{
	Matrix W = obj->GetTransform()->GetWorldMatrix();

	Vec3 vMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 vMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	auto accumulate = [&](const BoundingBox& box, const Matrix& m)
	{
		Vec3 corners[8];
		box.GetCorners(corners);
		for (int32 i = 0; i < 8; ++i)
		{
			Vec3 p = XMVector3TransformCoord(corners[i], m);
			vMin = ::XMVectorMin(vMin, p);
			vMax = ::XMVectorMax(vMax, p);
		}
	};

	shared_ptr<Model> model = nullptr;
	if (obj->GetModelRenderer())
		model = obj->GetModelRenderer()->GetModel();
	else if (obj->GetModelAnimator())
		model = obj->GetModelAnimator()->GetModel();

	if (model)
	{
		// 정적 모델은 렌더 시 메시별 본 트랜스폼(PushModelBoneData)이 곱해진다 —
		// 정점 원공간 AABB(CalculateModelBoundingBox)만 쓰면 본에 회전/이동이 박힌 모델
		// (예: Tower, 본 Rotation 90°)이 프레임에서 벗어남. 렌더와 동일하게 본을 적용해 집계.
		// (ModelAnimator 는 스키닝이 본을 적용하므로 원공간 AABB ≈ 바인드포즈)
		const bool useStaticBone = obj->GetModelRenderer() != nullptr;

		for (auto& mesh : model->GetMeshes())
		{
			BoundingBox box;
			box.Center  = 0.5f * (mesh->aabb.min + mesh->aabb.max);
			box.Extents = 0.5f * (mesh->aabb.max - mesh->aabb.min);

			Matrix m = W;
			if (useStaticBone && mesh->bone)
				m = mesh->bone->transform * W;

			accumulate(box, m);
		}
	}
	else
	{
		// 머티리얼 프리뷰 구체 등 — 단위 박스(반지름 0.5)
		BoundingBox unitBox;
		unitBox.Center = Vec3::Zero;
		unitBox.Extents = Vec3(0.5f, 0.5f, 0.5f);
		accumulate(unitBox, W);
	}

	if (vMin.x > vMax.x) // 메시 없는 모델 등 — 집계 실패 시 원점 단위 박스로 폴백
	{
		vMin = Vec3(-0.5f, -0.5f, -0.5f);
		vMax = Vec3(0.5f, 0.5f, 0.5f);
	}

	Vec3 center = 0.5f * (vMin + vMax);
	Vec3 extents = 0.5f * (vMax - vMin);
	float radius = max(extents.Length(), 0.01f);

	// 3/4 시점에서 중심 주시 — 외접구가 fov 에 꽉 차는 거리 + 여유 (애니 포즈 변형 감안)
	const float fov = XM_PI / 4.f;
	const float dist = radius / sinf(fov * 0.5f) * 1.1f;

	Vec3 dir = Vec3(0.35f, -0.3f, 1.f);
	dir.Normalize();
	Vec3 eye = center - dir * dist;

	outV = ::XMMatrixLookAtLH(eye, center, Vec3(0.f, 1.f, 0.f));
	outP = ::XMMatrixPerspectiveFovLH(fov, aspect, 0.01f, dist + radius * 4.f);
}

MeshThumbnail::~MeshThumbnail()
{

}

void MeshThumbnail::Draw(vector<shared_ptr<Renderer>> renderers, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<class InstancingBuffer>> buffers, bool drawGrid)
{
	if (renderers.size() == 0 || light == nullptr)
		return;

	_vp.RSSetViewport();
	Color color = Color(0.3f, 0.3f, 0.3f, 0.7f);
	DCT->OMSetRenderTargets(1, _colorMapRTV.GetAddressOf(), _depthMapDSV.Get());
	DCT->ClearRenderTargetView(_colorMapRTV.Get(), (float*)&color);
	DCT->ClearDepthStencilView(_depthMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	for (int32 i = 0; i < (int32)renderers.size(); ++i)
	{
		RenderContext ctx;
		ctx.tech   = renderers[i]->GetTechnique();
		ctx.view   = V;
		ctx.proj   = P;
		ctx.light  = light;
		ctx.buffer = buffers[i];

		// 머티리얼 프리뷰 구체(MeshRenderer)는 PS_PreviewLit 강제
		// — 씬용 Standard_PS 는 섀도우맵(미바인딩=0)/라이트배열 의존이라 썸네일이 검게 나옴
		if (renderers[i]->GetRenderType() == RendererType::Mesh)
			ctx.hlslOverride = RESOURCES->Get<HlslShader>(L"MeshPreview_HLSL");

		renderers[i]->Draw(ctx);
	}

	// 바닥 그리드 — 모델/애니 프리뷰에만 (머티리얼 구체엔 안 깐다). 모델이 깊이를 쓴 뒤
	// 깔아야 본체에 가려진 라인이 올바르게 컬링된다.
	bool hasModel = false;
	for (auto& r : renderers)
	{
		if (r && (r->GetRenderType() == RendererType::Model || r->GetRenderType() == RendererType::Animator))
		{
			hasModel = true;
			break;
		}
	}
	if (hasModel && drawGrid)
		DrawGrid(V, P, light);

	// 즉시 렌더(ImGui 업데이트 중 호출)이므로 메인 RT 복원 필수
	GRAPHICS->RestoreMainRenderTarget();
}

void MeshThumbnail::DrawGrid(Matrix V, Matrix P, shared_ptr<Light> light)
{
	if (_grid == nullptr)
	{
		_gridObj = make_shared<GameObject>();
		_gridObj->GetOrAddTransform(); // 원점 고정 (씬 그리드처럼 카메라 추적은 안 함)
		_gridObj->SetEditorInternal(true);
		_grid = make_shared<SceneGrid>();
		_gridObj->AddComponent(_grid);

		// 프리뷰 모델은 최대 축 기준 ~2유닛으로 정규화됨 — 0.5 셀(모델 폭 ~4칸),
		// 카메라 거리 페이드로 가장자리는 부드럽게 사라진다 (씬 그리드와 동일 룩)
		_grid->Init(60, 0.5f, 2.5f, 7.0f, 0.4f);
	}

	RenderContext ctx;
	ctx.view  = V;
	ctx.proj  = P;
	ctx.light = light;
	_grid->Draw(ctx);
}

void MeshThumbnail::CreateColorTexture()
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = _width;
	texDesc.Height = _height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> colorMap;

	HRESULT hr = DEVICE->CreateTexture2D(&texDesc, nullptr, colorMap.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateRenderTargetView(colorMap.Get(), nullptr, &_colorMapRTV);
	CHECK(hr);

	hr = DEVICE->CreateShaderResourceView(colorMap.Get(), nullptr, _shaderResourveView.GetAddressOf());
	CHECK(hr);
}

void MeshThumbnail::CreateDepthStencilTexture()
{
	// 같은 크기 썸네일은 깊이버퍼 1장 공유 — 드로우가 순차 실행이고 매 Draw 마다 클리어하므로 안전
	// (개별 보유 시 깊이만 개당 1MB(512²) x 캐시 64 낭비)
	static map<uint64, ComPtr<ID3D11DepthStencilView>> sharedDepth;

	const uint64 key = (static_cast<uint64>(_width) << 32) | _height;
	auto found = sharedDepth.find(key);
	if (found != sharedDepth.end())
	{
		_depthMapDSV = found->second;
		return;
	}

	D3D11_TEXTURE2D_DESC desc = { 0 };
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = _width;
	desc.Height = _height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthMap;

	HRESULT hr = DEVICE->CreateTexture2D(&desc, nullptr, depthMap.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateDepthStencilView(depthMap.Get(), nullptr, &_depthMapDSV);
	CHECK(hr);

	sharedDepth[key] = _depthMapDSV;
}
