#include "pch.h"
#include "BVH.h"
#include "ModelMesh.h"
#include "Mesh.h"
#include "MathUtils.h"


void BVH::BuildBVH(const BoundingBox& box, vector<shared_ptr<ModelMesh>>& meshes)
{
	if(_node == nullptr)
		_node = make_shared<BVHNode>();

	_node->boundingBox = box;
	_node->meshes = meshes;
}


DirectX::BoundingBox BVH::Combine(const BoundingBox& box1, const BoundingBox& box2)
{
	Vec3 min1 = Vec3(box1.Center) - Vec3(box1.Extents);
	Vec3 max1 = Vec3(box1.Center) + Vec3(box1.Extents);
	Vec3 min2 = Vec3(box2.Center) - Vec3(box2.Extents);
	Vec3 max2 = Vec3(box2.Center) + Vec3(box2.Extents);

	Vec3 newMin = ::XMVectorMin(min1, min2);
	Vec3 newMax = ::XMVectorMax(max1, max2);

	BoundingBox newBox;
	newBox.Center = (newMin + newMax) * 0.5f;
	newBox.Extents = (newMax - newMin) * 0.5f;

	return newBox;
}
