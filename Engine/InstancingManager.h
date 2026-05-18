#pragma once
#include "InstancingBuffer.h"

class GameObject;
class Shader;
class Camera;
class Light;
struct RenderContext;

class InstancingManager
{
	DECLARE_SINGLE(InstancingManager);

public:
	// New signature: RenderContext 기반
	void Render(const RenderContext& baseCtx, vector<shared_ptr<GameObject>>& gameObjects);
	void Clear() { _buffers.clear(); }
	void ClearData();

private:
	// 레거시 시그니처 제거, private 메서드로 변경
	void RenderStaticObject(const RenderContext& baseCtx, vector<shared_ptr<GameObject>>& gameObjects);
	void RenderAnimRenderer(const RenderContext& baseCtx, vector<shared_ptr<GameObject>>& gameObjects);

	void AddData(InstanceID instanceId, InstancingData& data);

private:
	map<InstanceID/*instanceId*/, shared_ptr<InstancingBuffer>> _buffers;
};

