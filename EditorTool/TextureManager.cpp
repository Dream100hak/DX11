#include "pch.h"
#include "TextureManager.h"
#include "TextureRenderer.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "Camera.h"
#include "TesTerrain.h"

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

	_tesTerrain = make_shared<TesTerrain>();
	TesTerrain::InitInfo tii;

	tii.heightMapFilename = L"../Resources/Assets/Textures/Terrain/terrain.raw";

	tii.layerMapFilename0 = L"../Resources/Assets/Textures/Terrain/grass.dds";
	tii.layerMapFilename1 = L"../Resources/Assets/Textures/Terrain/darkdirt.dds";
	tii.layerMapFilename2 = L"../Resources/Assets/Textures/Terrain/stone.dds";
	tii.layerMapFilename3 = L"../Resources/Assets/Textures/Terrain/lightdirt.dds";
	tii.layerMapFilename4 = L"../Resources/Assets/Textures/Terrain/snow.dds";

	tii.blendMapFilename = L"../Resources/Assets/Textures/Terrain/blend.dds";
	tii.heightScale = 50.0f;
	tii.heightmapWidth = 2049;
	tii.heightmapHeight = 2049;
	tii.cellSpacing = 0.5f;

	_tesTerrain->Init(tii);


	D3D11_RASTERIZER_DESC wireframeDesc;
	ZeroMemory(&wireframeDesc, sizeof(D3D11_RASTERIZER_DESC));
	wireframeDesc.FillMode = D3D11_FILL_WIREFRAME;
	wireframeDesc.CullMode = D3D11_CULL_BACK;
	wireframeDesc.FrontCounterClockwise = false;
	wireframeDesc.DepthClipEnable = true;

	HRESULT hr = {};

	hr = DEVICE->CreateRasterizerState(&wireframeDesc, _wireframeRS.GetAddressOf());
	CHECK(hr);
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

	if (GetAsyncKeyState('1') & 0x8000)
		DCT->RSSetState(_wireframeRS.Get());

	_tesTerrain->Draw();

	DCT->RSSetState(0);
}

