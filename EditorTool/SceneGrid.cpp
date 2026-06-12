#include "pch.h"
#include "SceneGrid.h"
#include "GeometryHelper.h"
#include "HlslShader.h"
#include "RenderStateManager.h"
#include "Camera.h"

SceneGrid::SceneGrid() : Super(RendererType::Grid)
{
	// 디퍼드 라이팅 이후 Pass 3 에서 알파 블렌드로 그린다
	_renderQueue = RenderQueue::Transparent;
}

SceneGrid::~SceneGrid()
{

}

void SceneGrid::Init(int32 count, float size, float fadeStart, float fadeEnd, float alpha)
{
	_gridSize = size;
	_gridExtent = count * size * 0.5f;
	_params.fadeStart = fadeStart;
	_params.fadeEnd   = fadeEnd;
	_params.baseAlpha = alpha;

	_shader = RESOURCES->Get<HlslShader>(L"SceneGrid_HLSL");
	if (_shader)
	{
		_shader->SetBlendState(RENDER_STATES->GetBS(BlendStateType::AlphaBlend));
		// 깊이 읽기 전용 — 지오메트리에 가려지되 그리드가 깊이를 더럽히지 않게
		_shader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));
		_shader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullNone));
	}

	_geometry = make_shared<Geometry<VertexTextureData>>();
	GeometryHelper::CreateSceneGrid(_geometry, count, size);

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());

	auto go = GetGameObject();
	go->SetUIPickable(false);
	go->SetEnableOutline(false);
	go->SetIgnoredTransformEdit(true);
}

void SceneGrid::Update()
{
	// 카메라 XZ 를 셀 크기로 스냅해 추적 — 라인이 항상 정수 좌표에 머물러 무한 그리드처럼 보인다
	auto scene = CUR_SCENE;
	if (scene == nullptr)
		return;

	auto cam = scene->GetMainCamera(); // editorInternal 카메라 우선
	if (cam == nullptr)
		return;

	Vec3 camPos = cam->GetTransform()->GetPosition();
	Vec3 pos;
	pos.x = std::round(camPos.x / _gridSize) * _gridSize;
	pos.y = 0.f;
	pos.z = std::round(camPos.z / _gridSize) * _gridSize;
	GetTransform()->SetPosition(pos);
}

void SceneGrid::TransformBoundingBox()
{
	Vec3 pos = GetTransform()->GetPosition();
	_boundingBox.Center  = pos;
	_boundingBox.Extents = Vec3(_gridExtent, 1.f, _gridExtent);
}

void SceneGrid::Draw(const RenderContext& ctx)
{
	// 씬 컬러 전용 — GBuffer/그림자/SSAO 패스에는 참여하지 않는다
	if (ctx.deferredPass || ctx.shadowPass || ctx.ssaoPass)
		return;

	if (_shader == nullptr || _vertexBuffer == nullptr)
		return;

	if (_paramsCB == nullptr)
	{
		_paramsCB = make_shared<ConstantBuffer<GridParamsDesc>>();
		_paramsCB->Create();
	}
	_paramsCB->CopyData(_params);
	_shader->SetPSConstantBuffer(8, _paramsCB->GetComPtr().Get());

	_shader->PushGlobalData(ctx.view, ctx.proj);
	_shader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	_shader->DrawLineIndexed(_indexBuffer->GetCount());
}
