#include "pch.h"
#include "ModelMesh.h"
#include "MathUtils.h"

void ModelMesh::CreateBuffers()
{
	vertexBuffer = make_shared<VertexBuffer>();
	vertexBuffer->Create(geometry->GetVertices());
	indexBuffer = make_shared<IndexBuffer>();
	indexBuffer->Create(geometry->GetIndices());

	CalculateMeshBox();
}

void ModelMesh::CalculateMeshBox()
{
	auto& vertices = geometry->GetVertices();

	Vec3 vMin = Vec3(MathUtils::INF, MathUtils::INF, MathUtils::INF);
	Vec3 vMax = Vec3(-MathUtils::INF, -MathUtils::INF, -MathUtils::INF);

	for (uint32 i = 0; i < vertices.size(); i++)
	{
		vMin = ::XMVectorMin(vMin, vertices[i].position);
		vMax = ::XMVectorMax(vMax, vertices[i].position);
	}
	_meshBox.Center = 0.5f * (vMin + vMax);
	_meshBox.Extents = 0.5f * (vMax - vMin);
}
