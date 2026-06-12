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
	Particle,  // Stream-Output 파티클(ParticleSystem) 렌더 Transparent 큐 추가
	Grid,      // 에디터 씬 그리드 — Mesh 로 두면 GetMeshRenderer 가 잘못 캐스팅(UB)한다

	End,
};

class Renderer : public Component
{
	using Super = Component;

public:
	Renderer(RendererType type);
	virtual ~Renderer();

	// 렌더링 함수
	virtual void Draw(const RenderContext& ctx) {}

	// 렌더링 함수
	virtual bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) { return false; }

public:
	void SetPass(uint8 pass) { _pass = pass; }
	void SetTechnique(uint8 teq) { _teq = teq; }

	void SetShadowMap(shared_ptr<Texture> shadowMap)  { _shadowMap = shadowMap;}
	void SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv) { _ssaoMap = srv; }

public:
	virtual void TransformBoundingBox();

public:
	
	uint8 GetTechnique() { return _teq;}

	virtual InstanceID GetInstanceID();
	BoundingBox& GetBoundingBox() { return _boundingBox; }
	RendererType GetRenderType() { return _renderType; }

	// 기본 렌더 큐 — MeshRenderer 는 머티리얼 큐가 우선, 커스텀 렌더러(씬 그리드 등)는 이 값 사용
	RenderQueue GetRenderQueue() { return _renderQueue; }
	void SetRenderQueue(RenderQueue queue) { _renderQueue = queue; }

protected:

	uint8				_pass = 0;
	uint8				_teq = 0;
	BoundingBox			_boundingBox;

	RendererType		_renderType = RendererType::End;
	RenderQueue			_renderQueue = RenderQueue::Opaque;

	shared_ptr<Texture> _shadowMap = nullptr;
	ComPtr<ID3D11ShaderResourceView> _ssaoMap = nullptr;

};

