#pragma once

class Shader;
class HlslShader;
class Light;
class InstancingBuffer;
struct LightArrayDesc;

// -----------------------------------------------------------
// RenderContext
//  - Renderer::Draw() �� ���޵Ǵ� ���� ���ؽ�Ʈ
//  - buffer == nullptr �� ��� ���� (Single)
//  - buffer != nullptr �� ��� �ν��Ͻ� (Instanced)
//  - shaderOverride != nullptr �� ��� �ش� ���̴��� �������̵� (ShadowPass ��)
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
	
	shared_ptr<Shader>         shaderOverride = nullptr;
	shared_ptr<HlslShader>     hlslOverride= nullptr;
	shared_ptr<InstancingBuffer> buffer     = nullptr;

	bool deferredPass = false;
};
