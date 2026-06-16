#pragma once
#include "Renderer.h"

class D3D12Device;

// 씬 그리드 렌더러 — Transparent 큐(지오메트리 위, 알파). PSO/CB 는 D3D12Device 소유(_dev, friend).
class GridRenderer : public Renderer
{
public:
	GridRenderer() : Renderer(RendererType::Grid) { _renderQueue = RenderQueue::Transparent; }
	void Bind(D3D12Device* dev) { _dev = dev; }
	virtual void Draw(const RenderContext& ctx) override;

private:
	D3D12Device* _dev = nullptr;
};
