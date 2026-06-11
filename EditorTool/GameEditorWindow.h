#pragma once
#include "EditorWindow.h"

// Game 뷰 — 씬에 배치한 게임 카메라(비-에디터 Camera) 시점을 RT 로 렌더해 표시
// - 플레이 중: 씬뷰 위에 풀사이즈 Game 창
// - 편집 중: 게임 카메라 선택 시 씬뷰 우하단 미니 프리뷰 (유니티 카메라 프리뷰)
class GameEditorWindow : public EditorWindow
{
public:
	GameEditorWindow(Vec2 pos, Vec2 size);
	~GameEditorWindow();

	virtual void Init() override;
	virtual void Update() override;

	void ShowGameWindow();
	void ShowCameraPreview();

private:
	shared_ptr<GameObject> FindGameCamera();
	void CreateRenderTarget(uint32 width, uint32 height);
	void RenderGameView(shared_ptr<GameObject> camObj);

	bool _wasPlaying = false; // 플레이 진입 프레임 감지 (포커스 1회만)

private:
	ComPtr<ID3D11Texture2D> _texture;
	ComPtr<ID3D11RenderTargetView> _rtv;
	ComPtr<ID3D11DepthStencilView> _dsv;
	ComPtr<ID3D11ShaderResourceView> _srv;
	Viewport _viewport;

	uint32 _width = 800;
	uint32 _height = 530;
};
