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
	// 플레이 중에만 씬뷰 위에 표시
	if (TOOL->IsPlaying() == false)
		return;

	ShowGameWindow();
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
	ImGui::SetNextWindowFocus(); // 씬뷰 위로

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

void GameEditorWindow::RenderGameView(shared_ptr<GameObject> camObj)
{
	_viewport.RSSetViewport();
	DCT->OMSetRenderTargets(1, _rtv.GetAddressOf(), _dsv.Get());

	Color clearColor(0.05f, 0.05f, 0.07f, 1.f);
	DCT->ClearRenderTargetView(_rtv.Get(), (float*)&clearColor);
	DCT->ClearDepthStencilView(_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	auto camera = camObj->GetCamera();
	camera->SetWidth((float)_width);
	camera->SetHeight((float)_height);
	camera->UpdateMatrix();

	// v1 은 포워드 렌더 (디퍼드 PBR 패스는 씬뷰 백버퍼 전용 — Game 뷰 디퍼드는 추후)
	camera->SortGameObject();
	camera->Render_Forward();

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
