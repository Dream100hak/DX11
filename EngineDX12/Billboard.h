#pragma once
#include "Renderer.h"
#include "Transform.h"
#include "imgui.h"

// 카메라 정면 쿼드(스프라이트/마커/플레어). 파티클 빌보드 파이프라인(_particlePSO)으로 일괄 렌더.
// 자체 지오메트리 없음 — D3D12Device::RenderParticles 가 Billboard 컴포넌트를 인스턴스로 수집해 그림.
class Billboard : public Renderer
{
public:
	Billboard() : Renderer(RendererType::Billboard) { _renderQueue = RenderQueue::Transparent; }

	virtual void Draw(const RenderContext&) override {} // 일괄 렌더(RenderParticles)에서 처리
	virtual void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Billboard");
		ImGui::DragFloat("Size", &_size, 0.02f, 0.05f, 20.f);
		ImGui::ColorEdit3("Color", &_color.x);
		ImGui::DragFloat("Intensity", &_intensity, 0.05f, 0.f, 16.f);
	}

	float Size() const { return _size; }
	DirectX::XMFLOAT3 Tint() const { return { _color.x * _intensity, _color.y * _intensity, _color.z * _intensity }; }

	float _size = 1.0f;
	Vec3  _color{ 1.f, 0.9f, 0.5f };
	float _intensity = 1.0f;
};
