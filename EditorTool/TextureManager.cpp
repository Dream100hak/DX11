#include "pch.h"
#include "TextureManager.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "Material.h"
#include "Camera.h"


void TextureManager::Init()
{
	auto camera = CUR_SCENE->GetMainCamera()->GetCamera();

	_smap = make_shared<ShadowMap>(2048, 2048);
	_ssao = make_shared<Ssao>(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height, camera->GetFov(), camera->GetFar());

	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");

	mat->SetShadowMap(_smap);
	mat->SetSsaoMap(_ssao->GetAmbientPtr());

	// ?붾쾭洹??띿뒪泥섎뒗 ?꾩옱 ImGui::Image(EditorTool::DrawRenderTextures)濡??쒖떆?섎?濡?
	// FX11 湲곕컲 TextureRenderer 寃쎈줈(01. DebugTexture.fx)???ъ슜?섏? ?딆쓬 ???쒓굅.
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
}

