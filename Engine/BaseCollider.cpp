#include "pch.h"
#include "BaseCollider.h"

BaseCollider::BaseCollider(ColliderType colliderType)
	: Component(ComponentType::Collider), _colliderType(colliderType)
{

}

BaseCollider::~BaseCollider()
{

}

void BaseCollider::CreateBuffers()
{
	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());
}
