#pragma once

class HlslShader;
class Light;
class InstancingBuffer;
struct LightArrayDesc;

// -----------------------------------------------------------
// RenderContext
//  - Renderer::Draw() 占쏙옙 占쏙옙占쌨되댐옙 占쏙옙占쏙옙 占쏙옙占쌔쏙옙트
//  - buffer == nullptr 占쏙옙 占쏙옙占?占쏙옙占쏙옙 (Single)
//  - buffer != nullptr 占쏙옙 占쏙옙占?占싸쏙옙占싹쏙옙 (Instanced)
//  - hlslOverride != nullptr 占쏙옙 占쏙옙占?HlslShader 占쏙옙占?占쏙옙占쏙옙占쏙옙占싱듸옙
//  - lightArray != nullptr 占쏙옙 占쏙옙占?占쏙옙티 占쏙옙占쏙옙트 占썼열 占쏙옙占쏙옙
// -----------------------------------------------------------
struct RenderContext
{
	int32  tech = 0;

	Matrix view = Matrix::Identity;
	Matrix proj = Matrix::Identity;

	shared_ptr<Light>      light  = nullptr; // 占쏙옙占쏙옙 占쏙옙占쏙옙트 (占쏙옙占쏙옙 호환占쏙옙)
	shared_ptr<LightArrayDesc> lightArray = nullptr; // 占쏙옙티 占쏙옙占쏙옙트 占썼열

	shared_ptr<HlslShader>     hlslOverride= nullptr;
	shared_ptr<InstancingBuffer> buffer     = nullptr;

	bool deferredPass = false;
	bool shadowPass   = false; // ShadowMap depth-only ?⑥뒪 (HLSL: Shadow*_HLSL)
	bool ssaoPass     = false; // SSAO normal-depth ?⑥뒪 (HLSL: SsaoNormalDepth*_HLSL)
};
