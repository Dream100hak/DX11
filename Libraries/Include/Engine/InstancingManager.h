#pragma once
#include "InstancingBuffer.h"

class GameObject;


class InstancingManager
{
	DECLARE_SINGLE(InstancingManager);

public:
	
	void PreRender();
	void Render(vector<shared_ptr<GameObject>>& gameObjects);
	void Clear() { _buffers.clear(); }
	void ClearData();

private:

	void PreRenderMeshRenderer();
	void PreRenderModelRenderer();
	void PreRenderAnimRenderer();

	void RenderMeshRenderer(vector<shared_ptr<GameObject>>& gameObjects);
	void RenderModelRenderer(vector<shared_ptr<GameObject>>& gameObjects);
	void RenderAnimRenderer(vector<shared_ptr<GameObject>>& gameObjects);


private:
	void AddData(InstanceID instanceId, InstancingData& data);

private:
	map<InstanceID/*instanceId*/, shared_ptr<InstancingBuffer>> _buffers;
};

