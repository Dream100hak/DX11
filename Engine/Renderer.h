#pragma once
#include "Component.h"
#include "RenderContext.h"

class InstancingBuffer;

enum class RendererType : uint8
{
	Mesh,
	Model,
	Animator,
	Texture,
	Particle,  // Stream-Output ?뚰떚??(ParticleSystem) ??Transparent ?? ?먯껜 Draw

	End,
};

class Renderer : public Component
{
	using Super = Component;

public:
	Renderer(RendererType type);
	virtual ~Renderer();

	// 占쏙옙占쏙옙 占신깍옙: 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙
	virtual void Draw(const RenderContext& ctx) {}

	// 占쏙옙占쏙옙 占쏙옙占신쏙옙 (占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙) 占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙
	virtual bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) { return false; }

public:
	void SetPass(uint8 pass) { _pass = pass; }
	void SetTechnique(uint8 teq) { _teq = teq; }

	void SetShadowMap(shared_ptr<Texture> shadowMap)  { _shadowMap = shadowMap;}
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv) { _ssaoMap = srv; }

public:
	void TransformBoundingBox();

public:
	
	uint8 GetTechnique() { return _teq;}

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

