#pragma once

struct ModelBone
{
	wstring name;
	int32 index;
	int32 parentIndex;
	shared_ptr<ModelBone> parent; // Cache

	Matrix transform;
	vector<shared_ptr<ModelBone>> children; // Cache
	BoundingBox boundingBox;

};

// .mesh 포맷 호환 AABB (구 aiAABB 와 동일한 24바이트 레이아웃 — Assimp 제거 후 대체)
struct MeshAabb
{
	Vec3 min = Vec3::Zero;
	Vec3 max = Vec3::Zero;
};

struct ModelMesh
{
	void CreateBuffers();
	shared_ptr<Geometry<ModelVertexType>> GetGeometry() { return geometry; }

	wstring name;

	// Mesh
	shared_ptr<Geometry<ModelVertexType>> geometry = make_shared<Geometry<ModelVertexType>>();
	shared_ptr<VertexBuffer> vertexBuffer;
	shared_ptr<IndexBuffer> indexBuffer;

	// Material
	wstring materialName = L"";
	shared_ptr<Material> material; // Cache

	// Bones
	int32 boneIndex;
	shared_ptr<ModelBone> bone; // Cache;

	//AABB
	MeshAabb aabb;

};

