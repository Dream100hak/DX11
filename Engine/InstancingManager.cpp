#include "pch.h"
#include "InstancingManager.h"
#include "InstancingBuffer.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Transform.h"
#include "Camera.h"
#include "MathUtils.h"


void InstancingManager::Render(int32 tech, shared_ptr<Shader> shader , Matrix V, Matrix P, shared_ptr<Light> light,  vector<shared_ptr<GameObject>>& gameObjects)
{
	ClearData();

	RenderStaticObject(tech, shader, V, P, light, gameObjects);
	RenderAnimRenderer(tech, shader, V, P, light,  gameObjects);
}


void InstancingManager::RenderStaticObject(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	for (shared_ptr<GameObject>& gameObject : gameObjects)
	{
		if (gameObject->GetRenderer() == nullptr)
			continue;

		if (gameObject->GetRenderer()->GetRenderType() == RendererType::Animator)
			continue;

		if (gameObject->GetSkyBox() != nullptr)
		{
			tech = 0;
		}

		const InstanceID instanceId = gameObject->GetRenderer()->GetInstanceID();
		cache[instanceId].push_back(gameObject);
	}

	for (auto& pair : cache)
	{
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		{
			const InstanceID instanceId = pair.first;

			for (int32 i = 0; i < vec.size(); i++)
			{
				const shared_ptr<GameObject>& gameObject = vec[i];
				InstancingData data;
				data.world = gameObject->GetTransform()->GetWorldMatrix();
				data.isPicked = gameObject->GetUIPicked() ? 1 : 0;
				AddData(instanceId, data);
			}

			shared_ptr<InstancingBuffer>& buffer = _buffers[instanceId];
			vec[0]->GetRenderer()->RenderInstancing(tech, shader, V, P, light, buffer);
		}
	}
}


void InstancingManager::RenderAnimRenderer(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light,  vector<shared_ptr<GameObject>>& gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	for (shared_ptr<GameObject>& gameObject : gameObjects)
	{
		if (gameObject->GetRenderer() == nullptr)
			continue;

		if(gameObject->GetRenderer()->GetRenderType() != RendererType::Animator)
			continue;

		const InstanceID instanceId = gameObject->GetModelAnimator()->GetInstanceID();
		cache[instanceId].push_back(gameObject);
	}

	for (auto& pair : cache)
	{
		shared_ptr<InstancedTweenDesc> tweenDesc = make_shared<InstancedTweenDesc>();

		const vector<shared_ptr<GameObject>>& vec = pair.second;

		{
			const InstanceID instanceId = pair.first;

			for (int32 i = 0; i < vec.size(); i++)
			{
				const shared_ptr<GameObject>& gameObject = vec[i];
				InstancingData data;
				data.world = gameObject->GetTransform()->GetWorldMatrix();
				data.isPicked = gameObject->GetUIPicked() ? 1 : 0;
				AddData(instanceId, data);

				// INSTANCING
				gameObject->GetModelAnimator()->UpdateTweenData();
				tweenDesc->tweens[i] = gameObject->GetModelAnimator()->GetTweenDesc();
			}

			vec[0]->GetModelAnimator()->GetShader()->PushTweenData(*tweenDesc.get());

			shared_ptr<InstancingBuffer>& buffer = _buffers[instanceId];
			vec[0]->GetModelAnimator()->RenderInstancing(tech, shader, V, P, light, buffer);
		}
	}
}


void InstancingManager::AddData(InstanceID instanceId, InstancingData& data)
{
	if (_buffers.find(instanceId) == _buffers.end())
		_buffers[instanceId] = make_shared<InstancingBuffer>();

	_buffers[instanceId]->AddData(data);
}

void InstancingManager::ClearData()
{
	for (auto& pair : _buffers)
	{
		shared_ptr<InstancingBuffer>& buffer = pair.second;
		buffer->ClearData();
	}
}
