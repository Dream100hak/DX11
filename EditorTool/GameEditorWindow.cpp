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

	ImGui::SetNextWindowPos(gameDesc.pos); // ���� ��ܿ� ��ġ
	ImGui::SetNextWindowSize(gameDesc.size); // ũ�� ����
	ImGui::Begin("Game");
	// ���� ������ ���� �߰�
	ImGui::End();
}
