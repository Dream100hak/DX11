#pragma once

#include "ModelMesh.h"
#include "Mesh.h"
#include "Material.h"
#include "GeometryHelper.h"
#include "Camera.h"
#include "HlslShader.h"


struct BVHNode {

	BoundingBox boundingBox;
	shared_ptr<BVHNode> leftChild;
	shared_ptr<BVHNode> rightChild;
	vector<shared_ptr<ModelMesh>> meshes; // LEAF 노드일 때만 메시 보관

	void Start()
	{

		auto mat = RESOURCES->Get<Material>(L"Collider");
		if (mat == nullptr)
		{
			mat = make_shared<Material>();
			mat->SetHlslShader(RESOURCES->Get<HlslShader>(L"Collider_HLSL"));
			MaterialDesc& desc = mat->GetMaterialDesc();
			desc.diffuse = Vec4(0.f, 1.f, 0.f, 1.f);

			RESOURCES->Add(L"Collider", mat);
		}

		material = mat;

		geometry = make_shared<Geometry<VertexColorData>>();
		GeometryHelper::CreateAABB(geometry, mat->GetMaterialDesc().diffuse, boundingBox);

		vertexBuffer = make_shared<VertexBuffer>();
		vertexBuffer->Create(geometry->GetVertices());
		indexBuffer = make_shared<IndexBuffer>();
		indexBuffer->Create(geometry->GetIndices());
	}

	void Update()
	{
		if (material == nullptr || geometry == nullptr)
			return;

		auto shader = material->GetHlslShader();
		if (shader == nullptr)
			return;

		Matrix world;
		Matrix matScale = Matrix::CreateScale(Vec3(1,1,1));
		Matrix matTranslation = Matrix::CreateTranslation(boundingBox.Center);

		world = matScale * matTranslation;

		auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
		shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());
		shader->PushTransformData(TransformDesc{ world });

		vertexBuffer->PushData();
		indexBuffer->PushData();

		shader->DrawLineIndexed(indexBuffer->GetCount());
	}

	shared_ptr<Geometry<VertexColorData>> geometry;
	shared_ptr<VertexBuffer> vertexBuffer;
	shared_ptr<IndexBuffer> indexBuffer;

	shared_ptr<class Material> material;
	uint8 pass = 0;

};

class BVH
{

public:
	 void BuildBVH(const BoundingBox& box,  vector<shared_ptr<ModelMesh>>& meshes);
	 BoundingBox Combine(const BoundingBox& box1, const BoundingBox& box2);

	 shared_ptr<BVHNode>& GetNode() {return _node;}

	 bool IntersectsBVH(const Matrix W,  const Ray& ray, shared_ptr<BVHNode>& node, vector<shared_ptr<ModelBone>>& bones , Vec3& pickPos)
	 {
		 if (!node) {
			 return false;
		 }

		 // 레이와 바운딩박스 교차 판정
		 float distance;
		 if (!ray.Intersects(node->boundingBox, OUT distance)) {
			 return false;
		 }

		 // 리프 노드일 때 삼각형 교차 판정
		 if (!node->leftChild && !node->rightChild)
		 {
			 for (const auto& mesh : node->meshes)
			 {
				 Matrix boneWorldMatrix = bones[mesh->boneIndex]->transform * W;

				 const auto& vertices = mesh->GetGeometry()->GetVertices();
				 const auto& indices = mesh->GetGeometry()->GetIndices();

				 for (uint32 i = 0; i < indices.size() / 3; ++i)
				 {
					 uint32 i0 = indices[i * 3 + 0];
					 uint32 i1 = indices[i * 3 + 1];
					 uint32 i2 = indices[i * 3 + 2];

					 Vec3 v0 = XMVector3TransformCoord(vertices[i0].position, boneWorldMatrix);
					 Vec3 v1 = XMVector3TransformCoord(vertices[i1].position, boneWorldMatrix);
					 Vec3 v2 = XMVector3TransformCoord(vertices[i2].position, boneWorldMatrix);

					 if (ray.Intersects(v0, v1, v2, OUT distance))
					 {
						 pickPos = ray.position + ray.direction * distance;
						 return true;
					 }
				 }
			 }
			 // 리프 노드일 때 삼각형 교차 판정
			 return false;
		 }

		 return IntersectsBVH(W , ray, node->leftChild , bones , pickPos ) || IntersectsBVH(W , ray, node->rightChild, bones,pickPos);
	 }

private:

	 shared_ptr<BVHNode> _node = nullptr;
};

