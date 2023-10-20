#include "pch.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "Game.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Light.h"

MeshRenderer::MeshRenderer() : Super(ComponentType::MeshRenderer)
{

}

MeshRenderer::~MeshRenderer()
{

}

void MeshRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	auto shader = _material->GetShader();
	if (shader == nullptr)
		return;

	// GlobalData
	shader->PushGlobalData(Camera::S_MatView, Camera::S_MatProjection);

	// Light
	auto lightObj = SCENE->GetCurrentScene()->GetLight();
	if (lightObj)
		shader->PushLightData(lightObj->GetLight()->GetLightDesc());

	// Light
	_material->Update();

	// IA
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	buffer->PushData();

	shader->DrawIndexedInstanced(0, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
}

InstanceID MeshRenderer::GetInstanceID()
{
	return make_pair((uint64)_mesh.get(), (uint64)_material.get());
}