#include "pch.h"
#include "SnowBillboard.h"
#include "Material.h"
#include "Camera.h"
#include "MathUtils.h"

SnowBillboard::SnowBillboard(Vec3 extent, int32 drawCount /*= 100*/)
	: Super(ComponentType::SnowBillBoard)
{

	_desc.extent = extent;
	_desc.drawDistance = _desc.extent.z * 2.0f;
	_drawCount = drawCount;

	const int32 vertexCount = _drawCount * 4;
	_vertices.resize(vertexCount);

	for (int32 i = 0; i < _drawCount * 4; i += 4)
	{
		Vec2 scale = MathUtils::RandomVec2(0.1f, 0.5f);

		Vec3 position;
		position.x = MathUtils::Random(-_desc.extent.x, _desc.extent.x);
		position.y = MathUtils::Random(-_desc.extent.y, _desc.extent.y);
		position.z = MathUtils::Random(-_desc.extent.z, _desc.extent.z);

		Vec2 random = MathUtils::RandomVec2(0.0f, 1.0f);

		_vertices[i + 0].position = position;
		_vertices[i + 1].position = position;
		_vertices[i + 2].position = position;
		_vertices[i + 3].position = position;

		_vertices[i + 0].uv = Vec2(0, 1);
		_vertices[i + 1].uv = Vec2(0, 0);
		_vertices[i + 2].uv = Vec2(1, 1);
		_vertices[i + 3].uv = Vec2(1, 0);

		_vertices[i + 0].scale = scale;
		_vertices[i + 1].scale = scale;
		_vertices[i + 2].scale = scale;
		_vertices[i + 3].scale = scale;

		_vertices[i + 0].random = random;
		_vertices[i + 1].random = random;
		_vertices[i + 2].random = random;
		_vertices[i + 3].random = random;
	}

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_vertices, 0);

	const int32 indexCount = _drawCount * 6;
	_indices.resize(indexCount);

	for (int32 i = 0; i < _drawCount; i++)
	{
		_indices[i * 6 + 0] = i * 4 + 0;
		_indices[i * 6 + 1] = i * 4 + 1;
		_indices[i * 6 + 2] = i * 4 + 2;
		_indices[i * 6 + 3] = i * 4 + 2;
		_indices[i * 6 + 4] = i * 4 + 1;
		_indices[i * 6 + 5] = i * 4 + 3;
	}

	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_indices);
}

SnowBillboard::~SnowBillboard()
{

}

void SnowBillboard::Update()
{
	_desc.origin = CUR_SCENE->GetMainCamera()->GetTransform()->GetPosition();
	_desc.time = _elpasedTime;
	_elpasedTime += DT;

	auto shader = _material->GetShader();

	// Transform
	auto world = GetTransform()->GetWorldMatrix();
	shader->PushTransformData(TransformDesc{ world });

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	// GlobalData
	shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

	// SnowData
	shader->PushSnowData(_desc);

	// Light
	_material->Update();

	// IA
	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	shader->DrawIndexed(0, _pass, _drawCount * 6);
}