#include "pch.h"
#include "TextureManager.h"
#include "TextureRenderer.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "Camera.h"


void TextureManager::Init()
{
	auto camera = CUR_SCENE->GetMainCamera()->GetCamera();

	_smap = make_shared<ShadowMap>(2048, 2048);
	_ssao = make_shared<Ssao>(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height, camera->GetFov(), camera->GetFar());

	shared_ptr<Shader> debugShader = make_shared<Shader>(L"01. DebugTexture.fx");

	_smapDebugTexture = make_shared<TextureRenderer>();
	_smapDebugTexture->SetShader(debugShader);
	_smapDebugTexture->SetDiffuseMap(_smap->GetComPtr().Get());

	_ssaoAmbientDebugTexture = make_shared<TextureRenderer>();
	_ssaoAmbientDebugTexture->SetShader(debugShader);
	_ssaoAmbientDebugTexture->SetDiffuseMap(_ssao->GetAmbientPtr().Get());

	_ssaoNormalDebugTexture = make_shared<TextureRenderer>();
	_ssaoNormalDebugTexture->SetShader(debugShader);
	_ssaoNormalDebugTexture->SetDiffuseMap(_ssao->GetNormalDepthPtr().Get());

}

void TextureManager::Update()
{
	DrawTextureMap();
}

void TextureManager::DrawTextureMap()
{

	JOB_PRE_RENDER->DoPush([=]()
	{
		_smap->Draw();
	});
	JOB_RENDER->DoPush([=]()
	{
		_ssao->Draw();
	});

	Matrix W1(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.75f, -0.75f, 0.0f, 1.0f);

	//if (_smapDebugTexture)
	//	_smapDebugTexture->Update(W1);

	if (_ssaoAmbientDebugTexture)
		_ssaoAmbientDebugTexture->Update(W1);

	Matrix W2(
		2.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.25f, -0.75f, 0.0f, 1.0f);

	//if (_ssaoNormalDebugTexture)
		//_ssaoNormalDebugTexture->Update(W2);

	//if (GetAsyncKeyState('1') & 0x8000)
	//	DCT->RSSetState(_wireframeRS.Get());

	////_tesTerrain->Draw();

	//DCT->RSSetState(0);
}

