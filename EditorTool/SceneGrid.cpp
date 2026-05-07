#include "pch.h"
#include "SceneGrid.h"
#include "GeometryHelper.h"

SceneGrid::SceneGrid() : Super(RendererType::Mesh)
{

}
SceneGrid::~SceneGrid()
{

}

void SceneGrid::Init(int32 count, float size)
{
	_shader = make_shared<Shader>(L"01. SceneGrid.fx");

	_geometry = make_shared<Geometry<VertexTextureData>>();
	GeometryHelper::CreateSceneGrid(_geometry, count, size);

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());

	auto go = GetGameObject();
	go->SetUIPickable(false);
}

void SceneGrid::Update()
{
	
}

void SceneGrid::Draw(const RenderContext& ctx)
{
	DrawGrid(ctx.view, ctx.proj);
}

void SceneGrid::DrawGrid(Matrix V, Matrix P)
{
	GetGameObject()->SetIgnoredTransformEdit(true);

	if (_shader == nullptr)
		return;


	 Matrix world = GetTransform()->GetWorldMatrix();

	_shader->PushTransformData(TransformDesc{ world });
	_shader->PushGlobalData(V, P);

	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	_shader->DrawLineIndexed(0, _pass, _indexBuffer->GetCount());
}
