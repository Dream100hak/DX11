#pragma once
#include "Component.h"
#include "RenderContext.h"
#include "Define.h"
#include <DirectXCollision.h>

// DX11 Engine/Renderer.h 이식 (DX12 적응 — DX11 SRV/Texture 멤버 제거, 추후 DX12 리소스로).
enum class RendererType : uint8
{
	Mesh,
	Model,
	Animator,
	Texture,
	Particle,
	Grid,     // 에디터 씬 그리드 — Mesh 로 두면 GetMeshRenderer 가 잘못 캐스팅(UB)
	Foliage,  // 터레인 식생(잔디/나무) — 자체 베이크 메시, GetMeshRenderer 회피 → RT TLAS 자동 제외
	End,
};

// 인스턴싱 배칭 키 (같은 메시+머티리얼끼리 묶음). 기본 (0,0) = 비배칭.
struct InstanceID
{
	uint64 a = 0, b = 0;
	bool operator==(const InstanceID& o) const { return a == o.a && b == o.b; }
};

class Renderer : public Component
{
	using Super = Component;
public:
	Renderer(RendererType type) : Component(ComponentType::Renderer), _renderType(type) {}
	virtual ~Renderer() {}

	virtual void Draw(const RenderContext& ctx) {}
	virtual bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) { return false; }
	// 선택 아웃라인 — PSO/루트시그/CB 는 호출측이 세팅, 여기선 자기 VB/IB 바인드 + 드로우만.
	// 기본 no-op(아웃라인 대상 아님 — 그리드/파티클/식생 등). MeshRenderer/ModelAnimator 만 오버라이드.
	virtual void RecordOutline(ID3D12GraphicsCommandList4* cmd) {}

	virtual void TransformBoundingBox();        // 기본: 트랜스폼 월드 중심 1×1×1
	virtual InstanceID GetInstanceID();          // 기본: (0,0)

	DirectX::BoundingBox& GetBoundingBox() { return _boundingBox; }
	RendererType GetRenderType() { return _renderType; }
	RenderQueue  GetRenderQueue() { return _renderQueue; }
	void         SetRenderQueue(RenderQueue q) { _renderQueue = q; }

protected:
	DirectX::BoundingBox _boundingBox;
	RendererType         _renderType = RendererType::End;
	RenderQueue          _renderQueue = RenderQueue::Opaque;
};
