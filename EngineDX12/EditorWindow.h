#pragma once
#include "Common.h"

class D3D12Device;

// ───────────────────────────────────────────────────────────
// EditorWindow — 에디터 패널 베이스 (EditorTool 의 EditorWindow 대응).
// 각 패널은 이 클래스를 상속해 Update() 안에서 자기 ImGui::Begin/End 를 담당한다.
// 공유 상태(렌더/라이팅 파라미터·씬·선택)는 D3D12Device 백포인터(_dev)로 접근 —
// EditorTool 이 TOOL/CUR_SCENE 싱글톤으로 접근하는 것에 대응(RtDemo 는 디바이스가 허브).
// ───────────────────────────────────────────────────────────
class EditorWindow
{
public:
	virtual ~EditorWindow() {}
	virtual void Init() {}
	virtual void Update() = 0;     // 매 프레임 — ImGui 패널 구성
	void Bind(D3D12Device* dev) { _dev = dev; }

protected:
	D3D12Device* _dev = nullptr;
};
