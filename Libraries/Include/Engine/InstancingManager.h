#pragma once
#include "InstancingBuffer.h"

class GameObject;
class Shader;
class Camera;
class Light; 


class InstancingManager
{
	DECLARE_SINGLE(InstancingManager);

public:
	
	void Render(shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects);
	void Clear() { _buffers.clear(); }
	void ClearData();

public:

	void RenderMeshRenderer(shared_ptr<Shader> shader, Matrix V , Matrix P,  shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects);
	void RenderModelRenderer(shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects);
	void RenderAnimRenderer(shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, vector<shared_ptr<GameObject>>& gameObjects);


private:
	void AddData(InstanceID instanceId, InstancingData& data);

private:
	map<InstanceID/*instanceId*/, shared_ptr<InstancingBuffer>> _buffers;
};

