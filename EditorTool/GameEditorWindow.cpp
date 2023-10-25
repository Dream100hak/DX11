#include "pch.h"
#include "GameEditorWindow.h"

GameEditorWindow::GameEditorWindow()
{

}

GameEditorWindow::~GameEditorWindow()
{

}

void GameEditorWindow::Init()
{

}

void GameEditorWindow::Update()
{
	ShowGameWindow();
}

void GameEditorWindow::ShowGameWindow()
{
	GameWindowDesc gameDesc;

	ImGui::SetNextWindowPos(gameDesc.pos); // 왼쪽 상단에 위치
	ImGui::SetNextWindowSize(gameDesc.size); // 크기 설정
	ImGui::Begin("Game");
	// 게임 윈도우 내용 추가
	ImGui::End();
}
