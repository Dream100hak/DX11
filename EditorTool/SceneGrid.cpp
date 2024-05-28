#include "pch.h"
#include "SceneGrid.h"
#include "GeometryHelper.h"

SceneGrid::SceneGrid() : Super(RendererType::Mesh)
{

}
SceneGrid::~SceneGrid()
{

}

void SceneGrid::Init()
{
	_shader = make_shared<Shader>(L"01. SceneGrid.fx");

	_geometry = make_shared<Geometry<VertexTextureData>>();
	GeometryHelper::CreateSceneGrid(_geometry,  100 , 5.f);

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

void SceneGrid::Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light)
{

}

void SceneGrid::RenderInstancing(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer)
{
	DrawGrid(V, P);
}

void SceneGrid::RenderThumbnail(int32 tech, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer)
{
	DrawGrid(V,P);
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
