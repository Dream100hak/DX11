#pragma once

class HlslShader;
class Light;
class InstancingBuffer;
struct LightArrayDesc;

// -----------------------------------------------------------
// RenderContext
//  - Renderer::Draw() �� ���޵Ǵ� ���� ���ؽ�Ʈ
//  - buffer == nullptr �� ��� ���� (Single)
//  - buffer != nullptr �� ��� �ν��Ͻ� (Instanced)
//  - hlslOverride != nullptr �� ��� HlslShader ��� �������̵�
//  - lightArray != nullptr �� ��� ��Ƽ ����Ʈ �迭 ����
// -----------------------------------------------------------
struct RenderContext
{
	int32  tech = 0;

	Matrix view = Matrix::Identity;
	Matrix proj = Matrix::Identity;

	shared_ptr<Light>      light  = nullptr; // ���� ����Ʈ (���� ȣȯ��)
	shared_ptr<LightArrayDesc> lightArray = nullptr; // ��Ƽ ����Ʈ �迭

	shared_ptr<HlslShader>     hlslOverride= nullptr;
	shared_ptr<InstancingBuffer> buffer     = nullptr;

	bool deferredPass = false;
	bool shadowPass   = false; // ShadowMap depth-only 패스 (HLSL: Shadow*_HLSL)
	bool ssaoPass     = false; // SSAO normal-depth 패스 (HLSL: SsaoNormalDepth*_HLSL)
};
