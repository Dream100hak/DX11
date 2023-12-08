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
	_meshBox.Center =   0.5f * (vMin + vMax);
	_meshBox.Extents = 0.5f * (vMax - vMin);

}

void ModelBone::CalculateBoneBoundingBox(shared_ptr<ModelBone>& bone, const vector<shared_ptr<ModelMesh>>& meshes, const Matrix& world)
{
	Vec3 minPoint = Vec3(MathUtils::INF, MathUtils::INF, MathUtils::INF);
	Vec3 maxPoint = Vec3(-MathUtils::INF, -MathUtils::INF, -MathUtils::INF);

	Matrix boneWorldMatrix = bone->transform * world;

	for (const auto& mesh : meshes) {
		if (mesh->boneIndex == bone->index) {
			auto& vertices = mesh->GetGeometry()->GetVertices();
			for (const auto& vertex : vertices) {
				Vec3 transformedPosition = XMVector3TransformCoord(vertex.position, boneWorldMatrix);

				minPoint = XMVectorMin(minPoint, transformedPosition);
				maxPoint = XMVectorMax(maxPoint, transformedPosition);
			}
		}
	}

	bone->boundingBox.Center = (minPoint + maxPoint) * 0.5f;
	bone->boundingBox.Extents = (maxPoint - minPoint) * 0.5f;
}
