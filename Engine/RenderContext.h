#pragma once

class Shader;
class Light;
class InstancingBuffer;

// -----------------------------------------------------------
// RenderContext
//  - Renderer::Draw() 에 전달되는 렌더 컨텍스트
//  - buffer == nullptr → 단일 드로우 (Single)
//  - buffer != nullptr → 인스턴싱 드로우 (Instanced)
//  - shaderOverride != nullptr → 해당 셰이더로 강제 교체 (ShadowPass 등)
// -----------------------------------------------------------
struct RenderContext
{
	int32  tech = 0;

	Matrix view = Matrix::Identity;
	Matrix proj = Matrix::Identity;

	shared_ptr<Light>          light     = nullptr;
	shared_ptr<Shader>         shaderOverride = nullptr; // nullptr = 자체 셰이더 사용
	shared_ptr<InstancingBuffer> buffer         = nullptr; // nullptr = Single draw
};
