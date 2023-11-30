#pragma once
#include "Component.h"

enum class ColliderType
{
	Sphere,         
	AABB,
	OBB,
};

class BaseCollider : public Component
{
public:
	BaseCollider(ColliderType colliderType);
	virtual ~BaseCollider();

	virtual bool Intersects(Ray& ray, OUT float& distance) = 0;
	virtual bool Intersects(shared_ptr<BaseCollider>& other) = 0;

	ColliderType GetColliderType() { return _colliderType; }

	shared_ptr<VertexBuffer> GetVertexBuffer() { return _vertexBuffer; }
	shared_ptr<IndexBuffer> GetIndexBuffer() { return _indexBuffer; }

	void CreateBuffers();
	void SetOffset(Vec3 offset) { _offset = offset; }

protected:

	Vec3 _offset = { 0.f , 0.f, 0.f };
	ColliderType _colliderType;
	shared_ptr<Geometry<VertexColorData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;


};
