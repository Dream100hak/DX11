#include "pch.h"
#include "SceneGrid.h"
#include "GeometryHelper.h"
#include "Camera.h"

void SceneGrid::Start()
{
	_shader = make_shared<Shader>(L"01. SceneGrid.fx");

	_frustum = make_shared<Frustum>();
	_frustum->FinalUpdate();

	_geometry = make_shared<Geometry<VertexColorData>>();

	GeometryHelper::CreateSceneGrid(_geometry , Color(0.4f,0.4f,0.4f,0.4f));
	
	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());

	auto go = GetGameObject();
	go->SetUIPickable(false);
}

void SceneGrid::Update()
{
	DrawGrid();
}

void SceneGrid::DrawGrid()
{
	if (_geometry == nullptr)
		return;

	if (_shader == nullptr)
		return;

	 Matrix world = GetTransform()->GetWorldMatrix();

	 auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();

	_shader->PushTransformData(TransformDesc{ world });
	_shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	_shader->DrawLineIndexed(0, _pass, _indexBuffer->GetCount());

}

