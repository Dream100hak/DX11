#pragma once
#include "Common.h"
#include "EditorWindow.h"
#include <string>
#include <memory>
#include <unordered_map>

class D3D12Device;

// ───────────────────────────────────────────────────────────
// EditorManager — 에디터 윈도우 레지스트리 (EditorTool 의 EditorToolManager 대응).
//  · unordered_map<string, EditorWindow> 로 패널 소유, Init() 1회 + Update() 매 프레임 순회.
//  · Update() 는 메인 메뉴바 → 전체화면 도킹 호스트(기본 레이아웃 1회) → 각 윈도우 Update() → ImGui::Render().
// 선택 상태(SelEntity)·폴더 브라우저 상태는 현재 D3D12Device 가 보유(렌더 루프도 _sel 을 읽으므로) —
// 매니저는 윈도우 생명주기/도킹만 담당한다.
// ───────────────────────────────────────────────────────────
class EditorManager
{
public:
	void Init(D3D12Device* dev); // 윈도우 생성·바인드·등록·Init
	void Update();               // 메뉴바 + 도킹 + 윈도우 순회 + ImGui::Render

	const std::unordered_map<std::string, std::shared_ptr<EditorWindow>>& Windows() const { return _windows; }

private:
	template<typename T> void Add(const char* name);

	D3D12Device* _dev = nullptr;
	std::unordered_map<std::string, std::shared_ptr<EditorWindow>> _windows;
};
