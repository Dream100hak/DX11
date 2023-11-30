#include "pch.h"
#include "Mesh.h"
#include "GeometryHelper.h"
#include "MathUtils.h"

Mesh::Mesh() : Super(ResourceType::Mesh)
{

}

Mesh::~Mesh()
{

}

void Mesh::CreateQuad()
{
	_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateQuad(_geometry);
	CreateBuffers();
}

void Mesh::CreateCube()
{
	_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateCube(_geometry);
	CreateBuffers();
}

void Mesh::CreateGrid(int32 sizeX, int32 sizeZ)
{
	_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateGrid(_geometry, sizeX, sizeZ);
	CreateBuffers();
}

void Mesh::CreateSphere()
{
	_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateSphere(_geometry);
	CreateBuffers();
}

void Mesh::CreateBuffers()
{
	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());
	CalculateMeshBox();
}

void Mesh::CalculateMeshBox()
{
	auto& vertices = _geometry->GetVertices();

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