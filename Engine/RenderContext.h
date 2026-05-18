#pragma once

class Shader;
class HlslShader;
class Light;
class InstancingBuffer;
struct LightArrayDesc;

// -----------------------------------------------------------
// RenderContext
//  - Renderer::Draw() 에 전달되는 렌더 컨텍스트
//  - buffer == nullptr 일 경우 단일 (Single)
//  - buffer != nullptr 일 경우 인스턴싱 (Instanced)
//  - shaderOverride != nullptr 일 경우 해당 셰이더로 오버라이드 (ShadowPass 등)
//  - hlslOverride != nullptr 일 경우 HlslShader 기반 오버라이드
//  - lightArray != nullptr 일 경우 멀티 라이트 배열 전달
// -----------------------------------------------------------
struct RenderContext
{
	int32  tech = 0;

	Matrix view = Matrix::Identity;
	Matrix proj = Matrix::Identity;

	shared_ptr<Light>      light  = nullptr; // 단일 라이트 (하위 호환성)
	shared_ptr<LightArrayDesc> lightArray = nullptr; // 멀티 라이트 배열
	
	shared_ptr<Shader>         shaderOverride = nullptr; // nullptr = 재질 셰이더 사용
	shared_ptr<HlslShader>     hlslOverride= nullptr; // nullptr = 재질 셰이더 사용
	shared_ptr<InstancingBuffer> buffer     = nullptr; // nullptr = Single draw
};
