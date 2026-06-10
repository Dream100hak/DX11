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

	// 디버그 텍스처는 현재 ImGui::Image(EditorTool::DrawRenderTextures)로 표시하므로
	// FX11 기반 TextureRenderer 경로(01. DebugTexture.fx)는 사용하지 않음 — 제거.
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

