#include "pch.h"
#include "TextureManager.h"
#include "ShadowMap.h"
#include "PunctualShadowMap.h"
#include "Ssao.h"
#include "Material.h"
#include "Camera.h"


void TextureManager::Init()
{
	auto camera = CUR_SCENE->GetMainCamera()->GetCamera();

	_smap = make_shared<ShadowMap>(2048, 2048);
	_punctual = make_shared<PunctualShadowMap>(1024);
	_ssao = make_shared<Ssao>(GAME->GetSceneDesc().width, GAME->GetSceneDesc().height, camera->GetFov(), camera->GetFar());

	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");

	mat->SetShadowMap(_smap);
	mat->SetSpotShadowMap(_punctual);
	mat->SetSsaoMap(_ssao->GetAmbientPtr());

	// ?붾쾭洹??띿뒪泥섎뒗 ?꾩옱 ImGui::Image(EditorTool::DrawRenderTextures)濡??쒖떆?섎?濡?
	// FX11 湲곕컲 TextureRenderer 寃쎈줈(01. DebugTexture.fx)???ъ슜?섏? ?딆쓬 ???쒓굅.
}

void TextureManager::Update()
{
	// 드로우는 SceneManager 의 pre-render 콜백에서 수행 (EditorTool::Init 에서 등록)
}

// 씬 렌더 직전 호출 — 같은 프레임 데이터로 섀도우/SSAO 드로우
// (구 잡큐 경유는 다음 프레임 시작에 실행되어 그림자가 1프레임 늦었음)
void TextureManager::DrawTextureMap()
{
	_smap->Draw();
	_punctual->Draw(); // 스팟 그림자 슬롯 깊이
	_ssao->Draw();
	GRAPHICS->RestoreMainRenderTarget();
}

