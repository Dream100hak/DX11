#pragma once
#include "EditorWindow.h"

// 씬 뷰 윈도우 — 씬 렌더 타겟 + ImGuizmo 트랜스폼 기즈모 + 클릭 픽킹 + 드래그앤드롭 배치
class SceneWindow : public EditorWindow
{
	enum Mode
	{
		Local,
		World,
	};

public:
	SceneWindow(Vec2 pos , Vec2 size);
	~SceneWindow();

	virtual void Init() override;
	virtual void Update() override;
	void Render();

	void ShowSceneWindow();

	// 렌더 타겟 관련 함수
	void CreateRenderTarget(uint32 width, uint32 height);
	void RenderScene();

	// ImGuizmo 기반 트랜스폼 기즈모 (이동/회전/스케일 + 스냅 + F 포커스)
	void EditTransform();

	// 라이트 기즈모 — 방향(디렉셔널)/원뿔(스팟)/구(포인트) 와이어프레임 (유니티/언리얼식)
	void DrawLightGizmos();

	// 터레인 브러시 — 편집 모드일 때 카메라 레이↔지형 교차로 브러시 링 표시 + 좌클릭 드래그 스컬프팅
	void DrawTerrainBrush();

	// 씬 RT 이미지 표시 + SceneDesc(이미지 영역 rect)/카메라 출력 RT 갱신
	void DrawSceneImage();

private:
	shared_ptr<Transform> _tr;
	OPERATION _currentGizmoOperation = TRANSLATE;

	// 기즈모 사용/호버 중 픽킹 차단 플래그
	bool _bUsing = false;

	// 렌더 타겟 멤버
	ComPtr<ID3D11Texture2D> _sceneTexture;
	ComPtr<ID3D11RenderTargetView> _sceneRTV;
	ComPtr<ID3D11DepthStencilView> _sceneDSV;
	ComPtr<ID3D11ShaderResourceView> _sceneSRV;
	Viewport _sceneViewport;

	uint32 _sceneWidth = 800;
	uint32 _sceneHeight = 530;
};
