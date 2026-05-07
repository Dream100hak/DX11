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
#include "RenderContext.h"

void InstancingManager::Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects)
{
	ClearData();
	RenderStaticObject(tech, shader, V, P, light, gameObjects);
	RenderAnimRenderer(tech, shader, V, P, light, gameObjects);
}

void InstancingManager::RenderStaticObject(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetRenderer() == nullptr) continue;
		if (gameObject->GetRenderer()->GetRenderType() == RendererType::Animator) continue;

		const InstanceID id = gameObject->GetRenderer()->GetInstanceID();
		cache[id].push_back(gameObject);
	}

	for (auto& [id, vec] : cache)
	{
		for (auto& go : vec)
		{
			InstancingData data;
			data.world    = go->GetTransform()->GetWorldMatrix();
			data.isPicked = go->GetUIPicked() ? 1 : 0;
			AddData(id, data);
		}

		RenderContext ctx;
		ctx.tech       = tech;
		ctx.view   = V;
		ctx.proj           = P;
		ctx.light   = light;
		ctx.shaderOverride = shader;
		ctx.buffer         = _buffers[id];

		vec[0]->GetRenderer()->Draw(ctx);
	}
}

void InstancingManager::RenderAnimRenderer(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetRenderer() == nullptr) continue;
		if (gameObject->GetRenderer()->GetRenderType() != RendererType::Animator) continue;

		const InstanceID id = gameObject->GetModelAnimator()->GetInstanceID();
		cache[id].push_back(gameObject);
	}

	for (auto& [id, vec] : cache)
	{
		auto tweenDesc = make_shared<InstancedTweenDesc>();

		for (int32 i = 0; i < (int32)vec.size(); i++)
		{
			InstancingData data;
			data.world    = vec[i]->GetTransform()->GetWorldMatrix();
			data.isPicked = vec[i]->GetUIPicked() ? 1 : 0;
			AddData(id, data);

			vec[i]->GetModelAnimator()->UpdateTweenData();
			tweenDesc->tweens[i] = vec[i]->GetModelAnimator()->GetTweenDesc();
		}

		vec[0]->GetModelAnimator()->GetShader()->PushTweenData(*tweenDesc);

		RenderContext ctx;
		ctx.tech   = tech;
		ctx.view   = V;
		ctx.proj   = P;
		ctx.light  = light;
		ctx.buffer = _buffers[id];

		vec[0]->GetModelAnimator()->Draw(ctx);
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
	for (auto& [id, buffer] : _buffers)
		buffer->ClearData();
}
