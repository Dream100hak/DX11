#pragma once
#include "Component.h"

class InstancingBuffer;

enum class RendererType : uint8
{
	Mesh,
	Model,
	Animator,
	Texture,

	End,
};

class Renderer : public Component
{
	using Super = Component;

public:

	Renderer(RendererType type);
	virtual ~Renderer();

	virtual void Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light) {}
	virtual void RenderInstancing(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer) {}
	virtual void RenderThumbnail(int32 tech, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer) {}

	virtual bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) { return false; }

public:
	void SetPass(uint8 pass) { _pass = pass; }
	void SetTechnique(uint8 teq) { _teq = teq; }

	void SetShadowMap(shared_ptr<Texture> shadowMap)  { _shadowMap = shadowMap;}
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv) { _ssaoMap = srv; }

public:
	void TransformBoundingBox();

public:
	
	virtual InstanceID GetInstanceID();
	BoundingBox& GetBoundingBox() { return _boundingBox; }
	RendererType GetRenderType() { return _renderType; }

protected:

	uint8				_pass = 0;
	uint8				_teq = 0;
	BoundingBox			_boundingBox;

	RendererType		_renderType = RendererType::End;

	shared_ptr<Texture> _shadowMap = nullptr;
	ComPtr<ID3D11ShaderResourceView> _ssaoMap = nullptr;

};

