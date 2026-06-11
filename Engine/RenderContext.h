#pragma once

class HlslShader;
class Light;
class InstancingBuffer;
struct LightArrayDesc;

// -----------------------------------------------------------
// RenderContext
// - Renderer::Draw() 함수에 전달되는 렌더 패스 컨텍스트
// 단일 라이트 (하위 호환성)
// - buffer != nullptr이면 인스턴싱 (Instanced)
// - hlslOverride != nullptr이면 지정된 HlslShader 사용 (오버라이드)
// 멀티라이트 배열
// -----------------------------------------------------------
struct RenderContext
{
	int32  tech = 0;

	Matrix view = Matrix::Identity;
	Matrix proj = Matrix::Identity;

	shared_ptr<Light>      light  = nullptr; // 단일 라이트 (하위 호환성)
	shared_ptr<LightArrayDesc> lightArray = nullptr; // 멀티라이트 배열

	shared_ptr<HlslShader>     hlslOverride= nullptr;
	shared_ptr<InstancingBuffer> buffer     = nullptr;

	bool deferredPass = false;
	bool shadowPass   = false; // ShadowMap depth-only 패스 (HLSL: Shadow*_HLSL)
	bool ssaoPass     = false; // SSAO normal-depth 패스 (HLSL: SsaoNormalDepth*_HLSL)
};
