#pragma once
#include "EditorWindow.h"

// 터레인 에디터 창 — 브러시 스컬프팅(올림/내림/스무드/평탄화) 파라미터 + 유틸.
// 실제 브러시 입력/링 오버레이는 씬 뷰(SceneWindow)에서 처리하며, 아래 정적 상태를 읽는다.
class TerrainWindow : public EditorWindow
{
public:
	TerrainWindow(Vec2 pos, Vec2 size);
	virtual ~TerrainWindow();

	virtual void Init() override;
	virtual void Update() override;

	// SceneWindow 브러시가 읽는 편집 상태
	static bool  S_Edit;     // 편집 모드 on/off (켜지면 씬뷰 좌클릭이 픽킹 대신 스컬프팅)
	static int32 S_Mode;     // 0=Raise 1=Lower 2=Smooth 3=Flatten
	static float S_Radius;   // 브러시 월드 반경
	static float S_Strength; // 브러시 세기
};
