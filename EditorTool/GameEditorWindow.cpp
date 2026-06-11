#include "pch.h"
#include "GameEditorWindow.h"
#include "EditorToolManager.h"
#include "GameObject.h"
#include "Camera.h"

GameEditorWindow::GameEditorWindow(Vec2 pos, Vec2 size)
{
	SetWinPosAndSize(pos, size);
	_width = (uint32)size.x;
	_height = (uint32)size.y;
}

GameEditorWindow::~GameEditorWindow()
{

}

void GameEditorWindow::Init()
{
	CreateRenderTarget(_width, _height);
}

void GameEditorWindow::Update()
{
	const bool playing = TOOL->IsPlaying();

	if (playing)
		ShowGameWindow();       // 풀사이즈 Game 뷰
	else
		ShowCameraPreview();    // 게임 카메라 선택 시 우하단 미니 프리뷰

	_wasPlaying = playing;
}

// 씬에서 게임 카메라 탐색 — 에디터 내부가 아닌 첫 Camera 오브젝트
shared_ptr<GameObject> GameEditorWindow::FindGameCamera()
{
	for (auto& [id, obj] : CUR_SCENE->GetCreatedObjects())
	{
		if (obj == nullptr || obj->IsEditorInternal())
			continue;
		if (obj->GetCamera() != nullptr)
			return obj;
	}
	return nullptr;
}

void GameEditorWindow::ShowGameWindow()
{
	shared_ptr<GameObject> camObj = FindGameCamera();

	if (camObj != nullptr)
		RenderGameView(camObj);

	ImGui::SetNextWindowPos(GetEWinPos());
	ImGui::SetNextWindowSize(GetEWinSize());

	// 포커스는 플레이 진입 첫 프레임만 — 매 프레임 강탈하면 하이라키 우클릭 메뉴 등이 즉시 닫힘
	if (_wasPlaying == false)
		ImGui::SetNextWindowFocus();

	ImGui::Begin("Game", nullptr,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

	if (camObj == nullptr)
	{
		ImGui::Dummy(ImVec2(0, 40.f));
		ImGui::SetCursorPosX(40.f);
		ImGui::TextDisabled("No Game Camera in scene.");
		ImGui::SetCursorPosX(40.f);
		ImGui::TextDisabled("Hiearchy > GameObject > Create Camera");
	}
	else
	{
		ImGui::Image(_srv.Get(), ImGui::GetContentRegionAvail());
	}

	ImGui::End();
}

// 편집 중 카메라 프리뷰 — 게임 카메라(비-에디터 Camera) 선택 시 씬뷰 우하단 인셋
void GameEditorWindow::ShowCameraPreview()
{
	int64 id = TOOL->GetSelectedIdH();
	if (id == -1)
		return;

	// 다른 오브젝트를 선택하면 닫기 상태 해제 (다시 카메라 선택 시 재표시)
	if (id != _lastPreviewId)
	{
		_previewHidden = false;
		_lastPreviewId = id;
	}
	if (_previewHidden)
		return;

	shared_ptr<GameObject> obj = CUR_SCENE->GetCreatedObject((int32)id);
	if (obj == nullptr || obj->IsEditorInternal() || obj->GetCamera() == nullptr)
		return;

	RenderGameView(obj);

	const SceneDesc& scene = GAME->GetSceneDesc();
	const float pw = 320.f;
	const float ph = pw * ((float)_height / (float)_width) + 30.f; // 타이틀 줄 여유

	ImGui::SetNextWindowPos(ImVec2(scene.x + scene.width - pw - 12.f, scene.y + scene.height - ph - 12.f));
	ImGui::SetNextWindowSize(ImVec2(pw, ph));
	ImGui::SetNextWindowBgAlpha(0.9f);

	ImGui::Begin("CameraPreview", nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

	ImGui::TextDisabled("Camera Preview");
	ImGui::SameLine(pw - 34.f);
	if (ImGui::SmallButton("x"))
		_previewHidden = true;

	ImGui::Image(_srv.Get(), ImGui::GetContentRegionAvail());

	ImGui::End();
}

void GameEditorWindow::RenderGameView(shared_ptr<GameObject> camObj)
{
	Color clearColor(0.05f, 0.05f, 0.07f, 1.f);
	DCT->ClearRenderTargetView(_rtv.Get(), (float*)&clearColor);

	auto camera = camObj->GetCamera();
	camera->SetWidth((float)_width);
	camera->SetHeight((float)_height);
	camera->UpdateMatrix();

	// 씬뷰와 동일한 디퍼드 풀파이프라인 (터레인/그림자/PBR/블룸/FXAA) — 최종 출력만 이 RT 로
	camera->SetFinalOutput(_rtv);
	camera->SortGameObject();
	camera->Render_Deferred();
	camera->SetFinalOutput(nullptr);

	GRAPHICS->RestoreMainRenderTarget();
}

void GameEditorWindow::CreateRenderTarget(uint32 width, uint32 height)
{
	_width = width;
	_height = height;

	// Color RT + SRV
	{
		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		CHECK(DEVICE->CreateTexture2D(&texDesc, nullptr, _texture.GetAddressOf()));
		CHECK(DEVICE->CreateRenderTargetView(_texture.Get(), nullptr, _rtv.GetAddressOf()));
		CHECK(DEVICE->CreateShaderResourceView(_texture.Get(), nullptr, _srv.GetAddressOf()));
	}

	// Depth
	{
		D3D11_TEXTURE2D_DESC depthDesc{};
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ComPtr<ID3D11Texture2D> depthTexture;
		CHECK(DEVICE->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf()));
		CHECK(DEVICE->CreateDepthStencilView(depthTexture.Get(), nullptr, _dsv.GetAddressOf()));
	}

	_viewport.Set((float)width, (float)height, 0, 0);
}
