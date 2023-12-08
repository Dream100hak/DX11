#pragma once

#include "ModelMesh.h"
#include "Mesh.h"
#include "Material.h"
#include "GeometryHelper.h"
#include "Camera.h"


struct BVHNode {

	BoundingBox boundingBox;
	shared_ptr<BVHNode> leftChild;
	shared_ptr<BVHNode> rightChild;
	vector<shared_ptr<ModelMesh>> meshes; // LEAF 노드에 대한 메시 목록

	void Start()
	{

		auto mat = RESOURCES->Get<Material>(L"Collider");
		if (mat == nullptr)
		{
			mat = make_shared<Material>();
			auto shader = make_shared<Shader>(L"01. Collider.fx");
			mat->SetShader(shader);
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

		auto shader = material->GetShader();
		if (shader == nullptr)
			return;

		Matrix world;
		Matrix matScale = Matrix::CreateScale(Vec3(1,1,1));
		Matrix matTranslation = Matrix::CreateTranslation(boundingBox.Center);

		world = matScale * matTranslation;

		shader->PushTransformData(TransformDesc{ world });

		auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
		// GlobalData
		shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

		vertexBuffer->PushData();
		indexBuffer->PushData();

		shader->DrawLineIndexed(0, pass, indexBuffer->GetCount());
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

		 // 레이와 노드의 바운딩 볼륨 교차 검사
		 float distance;
		 if (!ray.Intersects(node->boundingBox, OUT distance)) {
			 return false;
		 }

		 // 잎 노드인 경우, 메시들과의 교차 검사
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
			 // 리프 노드의 모든 메시와 교차하지 않는 경우
			 return false;
		 }

		 return IntersectsBVH(W , ray, node->leftChild , bones , pickPos ) || IntersectsBVH(W , ray, node->rightChild, bones,pickPos);
	 }

private:

	 shared_ptr<BVHNode> _node = nullptr;
};

