#pragma once
#include "Renderer.h"

class D3D12Device;

// 절차적 스카이박스 렌더러 — Background 큐. PSO/루트시그/CB 는 D3D12Device 소유(_dev, friend).
class SkyRenderer : public Renderer
{
public:
	SkyRenderer() : Renderer(RendererType::Texture) { _renderQueue = RenderQueue::Background; }
	void Bind(D3D12Device* dev) { _dev = dev; }
	virtual void Draw(const RenderContext& ctx) override;

private:
	D3D12Device* _dev = nullptr;
};
