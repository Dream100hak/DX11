#pragma once
#include "Renderer.h"

class D3D12Device;

// 모델(스키닝 메시) + 바닥 지오메트리 렌더러 컴포넌트.
// 실제 지오메트리/머티리얼은 D3D12Device 의 ModelScene(_scene)에 있고, PSO·루트시그·CB·DDGI 도
// 디바이스 소유 → 백포인터(_dev, friend)로 접근해 Draw(ctx) 에서 불투명 드로우를 기록한다.
// (DX11 Engine 의 ModelAnimator/MeshRenderer 에 대응 — 전환 단계 통합 렌더러)
class ModelRenderer : public Renderer
{
public:
	ModelRenderer() : Renderer(RendererType::Model) {}
	void Bind(D3D12Device* dev) { _dev = dev; }

	virtual void Draw(const RenderContext& ctx) override; // 불투명: 모델 서브메시 + 바닥
	virtual void TransformBoundingBox() override;          // ModelScene 월드 AABB 사용

private:
	D3D12Device* _dev = nullptr;
};
