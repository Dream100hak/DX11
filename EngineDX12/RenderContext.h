#pragma once
#include "Common.h"

// DX11 Engine/RenderContext.h 이식 (DX12 적응).
// Renderer::Draw() 에 전달되는 패스 컨텍스트. DX12 는 즉시 컨텍스트 대신 커맨드리스트에 기록하므로 cmd 포함.
struct RenderContext
{
	int32  tech = 0;

	Matrix view{};
	Matrix proj{};

	ID3D12GraphicsCommandList4* cmd = nullptr; // 드로우 기록 대상 (DX11 의 DCT 대응)

	bool deferredPass = false; // GBuffer 채우기 패스
	bool shadowPass   = false; // ShadowMap depth-only 패스
	bool ssaoPass     = false; // SSAO normal-depth 패스
};
