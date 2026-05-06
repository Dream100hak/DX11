    <!-- 파일 인코딩: UTF-8 (BOM 없음) | 편집 시 반드시 UTF-8로 저장할 것 -->
<!-- VS Code: 우하단 인코딩 클릭 → 'UTF-8' 선택 | Visual Studio: 파일 → 고급 저장 옵션 → UTF-8 -->

# DX11 Editor Tool ? 코드 리뷰 & 개선 보고서

---

## 목차
1. [코드 규칙 (Coding Conventions)](#1-코드-규칙)
2. [아키텍처 / 설계 문제](#2-아키텍처--설계-문제)
3. [버그 & 잠재적 크래시](#3-버그--잠재적-크래시)
4. [성능 문제](#4-성능-문제)
5. [ImGui 사용 문제](#5-imgui-사용-문제)
6. [EditorTool 구조 개선](#6-editortool-구조-개선)
7. [기능 누락 / UX 개선](#7-기능-누락--ux-개선)
8. [우선순위 요약](#8-우선순위-요약)

---

## 1. 코드 규칙

프로젝트 전반에 걸쳐 아래 규칙이 **혼재하거나 누락**되어 있습니다.  
일관성을 위해 다음을 기준으로 통일하는 것을 권장합니다.

### 1-1. 네이밍 컨벤션

| 항목 | 현재 상태 | 권장 규칙 |
|------|-----------|-----------|
| 멤버 변수 | `_camelCase` (대부분 지킴) | `_camelCase` 유지 |
| 매크로 | `TOOL`, `SELECTED_H` 등 ALL_CAPS | ALL_CAPS 유지 |
| 클래스 | `PascalCase` (대부분 지킴) | `PascalCase` 유지 |
| 오타 | `Hiearchy` → **Hierarchy**, `_cashesFileList` → **_cachedFileList**, `_cashesModelList` → **_cachedModelList** | 오타 수정 |
| 오타 | `_meshthumbnail` → **_meshThumbnail** | 멤버 변수 camelCase |

```cpp
// 현재 ? 오타
map<wstring, shared_ptr<MetaData>> _cashesFileList;   // ? cashes → cached
class Hiearchy : public EditorWindow                  // ? Hiearchy → Hierarchy

// 개선
map<wstring, shared_ptr<MetaData>> _cachedFileList;   // ?
class Hierarchy : public EditorWindow                 // ?
```

### 1-2. 매크로 남용

`Define.h`에 상태 접근 매크로가 난발되어 있어 코드 추적이 어렵습니다.

```cpp
// 현재 ? 매크로로 상태 접근
#define SELECTED_H   TOOL->GetSelectedIdH()
#define SELECTED_P   TOOL->GetSelectedIdP()

// 개선 ? 직접 호출하거나 const ref 사용
// 코드 내에서 TOOL->GetSelectedIdH() 로 명시적 호출
// 또는 using 선언으로 alias 대신 함수 래퍼 사용
```

### 1-3. 헤더 내 함수 구현 (인라인 비대화)

`EditorToolManager.h`, `FolderContents.h` 등에 헤더에 직접 구현된 함수들이 많습니다.  
짧은 getter가 아닌 로직이 있는 함수는 `.cpp`로 분리해야 합니다.

```cpp
// EditorToolManager.h ? 헤더에 구현된 복잡한 로직 ?
MetaType GetMetaType(const wstring& name) { /* 30줄 */ }

// FolderContents.h ? 헤더에 구현 ?
std::string AdjustItemNameToFit(...) { /* 로직 */ }
std::wstring CreateUniqueMaterialName(...) { /* 로직 */ }
std::wstring GetExecutablePath() { /* 로직 */ }
```

### 1-4. `OUT` 키워드

`OUT` 매크로를 파라미터에 사용하는 패턴이 일부 존재합니다.  
C++20 표준과 맞지 않으니 `[[maybe_unused]]` 또는 레퍼런스 타입으로 의도를 표현하세요.

```cpp
// 현재
void PickMaterialTexture(string textureType, OUT bool& changed);

// 개선 ? OUT 매크로 없이 레퍼런스 자체로 의미 전달
void PickMaterialTexture(const string& textureType, bool& outChanged);
```

---

## 2. 아키텍처 / 설계 문제

### 2-1. `EditorToolManager` ? 단일 책임 원칙 위반

`EditorToolManager`가 아래 역할을 동시에 담당합니다.
- 에디터 윈도우 레지스트리
- **선택 상태 관리** (`_selectedH`, `_selectedP`)
- **파일 캐시 관리** (`_cashesFileList`, `_cashesModelList`)
- **파일 타입 분류** (`GetMetaType`)

```
권장 분리:
├── EditorToolManager       ? 윈도우 등록/조회만
├── SelectionContext         ? 선택 상태 (새 클래스)
├── AssetCache               ? 파일 캐시 (새 클래스 or ResourceManager 활용)
└── AssetTypeResolver        ? 확장자→MetaType 변환 (유틸 함수)
```

### 2-2. `GetEditorWindow()` ? UB (Undefined Behavior)

```cpp
// EditorToolManager.h ? ? 못 찾으면 아무것도 return 하지 않음 (UB)
const shared_ptr<EditorWindow>& GetEditorWindow(string name)
{
    auto it = _editorWindows.find(name);
    if (it != _editorWindows.end())
    {
        return it->second;
    }
    // ← 여기서 return 없음! 컴파일러 경고, 런타임 UB
}

// 개선
const shared_ptr<EditorWindow>& GetEditorWindow(const string& name) const
{
    static const shared_ptr<EditorWindow> sNull = nullptr;
    auto it = _editorWindows.find(name);
    return (it != _editorWindows.end()) ? it->second : sNull;
}
```

### 2-3. `Inspector`와 `FolderContents`의 Preview 오브젝트 중복

`Inspector`와 `FolderContents` 모두 독립적으로 `_meshPreviewCamera`, `_meshPreviewLight`를 생성·보유합니다.  
씬 리소스 낭비이며 두 프리뷰가 동기화되지 않을 위험이 있습니다.

```
권장: PreviewSceneContext 공유 객체로 추출하거나
      Inspector가 FolderContents의 카메라/라이트를 참조하도록 단일화
```

### 2-4. `SceneWindow`/`GameEditorWindow` 파일 ? SceneWindow.h가 누락

`SceneWindow.h`가 검색되지 않고 내용이 출력되지 않았습니다.  
`SceneWindow`가 `Effects.h` 내부에 합쳐져 있거나 분리가 불명확한 것으로 보입니다.  
Scene 뷰 관련 클래스는 명확하게 분리되어야 합니다.

### 2-5. `EditorTool.h`에 불필요한 멤버

```cpp
shared_ptr<class Button> _btn;         // 에디터에서 Button 컴포넌트 직접 보유? 용도 불명확
shared_ptr<class ParticleSystem> _rainDrop;  // 에디터 메인 클래스에 씬 오브젝트 직접 소유
```
씬 오브젝트는 `Scene`이 소유해야 합니다. `EditorTool`은 초기화만 담당하고 소유권은 씬에 넘겨야 합니다.

---

## 3. 버그 & 잠재적 크래시

### 3-1. `Hiearchy.cpp` ? `Selectable` 두 번째 인자 버그

```cpp
// 현재 ? ? 쉼표 연산자로 isSelected가 무시됨
if (ImGui::Selectable(name.c_str(), (isSelected, ImGuiSelectableFlags_SpanAllColumns)))

// 개선 ?
if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
```
`(isSelected, flags)` 는 C++ 쉼표 연산자로 `isSelected`가 버려지고 항상 flags 값(=0이 아닌 정수)이 `bool`로 평가됩니다. 즉, **모든 항목이 항상 선택된 것처럼 표시**되는 버그입니다.

### 3-2. `GetEditorWindow()` 반환값 미보장 (UB)

위 2-2 항목 참조. 함수가 `nullptr`을 반환하지 않고 종료되면 **런타임 크래시** 발생.

### 3-3. `wstring → string` 변환 ? 비ASCII 문자 깨짐

```cpp
// 현재 ? 한글/한자 등 멀티바이트 문자 깨짐 ?
string name(wstr.begin(), wstr.end());

// 개선 ? WideCharToMultiByte 또는 std::filesystem::path 사용
string name = Utils::ToString(wstr);  // 프로젝트 내 Utils 활용
```
파일명이나 오브젝트 이름에 한글이 들어가면 ImGui에 잘못된 문자열이 전달됩니다.

### 3-4. `Inspector::Init()` ? 매 호출 시 중복 생성 가능

`Init()`이 에디터 윈도우 재초기화 시 다시 호출되면 `_meshPreviewCamera` 등을 재생성합니다.  
현재는 `if (_meshPreviewCamera == nullptr)` 가드가 있어 안전하지만,  
`_sceneGrids`는 `size() == 0` 체크로 보호되어 있어 **리셋 후 재초기화 시 중복 추가** 가능성 있음.

```cpp
// 개선 ? Init() 시작 시 명시적 클리어
_sceneGrids.clear();
```

---

## 4. 성능 문제

### 4-1. `GetMetaType()` ? 매 프레임 호출 시 비효율

확장자 비교를 `if-else if` 체인으로 처리합니다.  
파일 목록이 많을 때 매 프레임 호출되면 낭비입니다.

```cpp
// 개선 ? static unordered_map 룩업 테이블
static const unordered_map<wstring, MetaType> extMap = {
    {L"txt", MetaType::TEXT}, {L"TXT", MetaType::TEXT},
    {L"wav", MetaType::SOUND}, {L"mp3", MetaType::SOUND},
    {L"jpg", MetaType::IMAGE}, {L"png", MetaType::IMAGE}, {L"dds", MetaType::IMAGE},
    // ...
};
// 확장자를 소문자로 정규화 후 룩업
```

### 4-2. 파일 캐시 (`_cashesFileList`) ? `map` 대신 `unordered_map`

`map<wstring, ...>` 은 O(log n) 탐색. 파일이 많으면 `unordered_map`이 O(1).  
이미 `_editorWindows`는 `unordered_map`을 쓰면서 파일 리스트만 `map`을 씁니다.

### 4-3. `FolderContents::AdjustItemNameToFit()` ? 매 아이템 렌더마다 반복 문자 제거

```cpp
// 현재 ? 매 프레임, 매 아이템마다 CalcTextSize를 반복 호출
while (!adjustedName.empty() && (ImGui::CalcTextSize(...).x + ellipsisTextSize > buttonWidth)) {
    adjustedName.pop_back();  // 한 글자씩 제거
}
```
이진 탐색으로 교체하거나 결과를 캐싱해야 합니다.

### 4-4. 썸네일 Preview 오브젝트 무제한 증가

`FolderContents::_meshPreviewObjs` / `_meshPreviewthumbnails`는 `unordered_map`에 계속 추가됩니다.  
파일 수가 많아지면 메모리가 무제한 증가합니다. **LRU 캐시** 또는 최대 N개 제한이 필요합니다.

---

## 5. ImGui 사용 문제

### 5-1. 윈도우 위치/크기 하드코딩

```cpp
// Main.cpp ? 픽셀 하드코딩
sceneDesc.width = 800;
sceneDesc.height = 530;

// Inspector.cpp ? 하드코딩된 오프셋
ImGui::SetNextWindowPos(GetEWinPos(), ImGuiCond_Appearing);
```
해상도가 바뀌면 레이아웃이 깨집니다.  
`ImGuiCond_Always` 대신 `ImGuiCond_FirstUseEver` + INI 저장 활용, 또는 비율 기반 레이아웃 도입.

### 5-2. `ImGui::Begin` 이후 조기 리턴 누락

```cpp
// 일부 윈도우 ? ? Begin()이 false 반환해도 End() 없이 진행
ImGui::Begin("Hiearchy", nullptr);
// ... 내용 ...
// ImGui::End() 가 없거나 Begin 리턴값 미확인
```
`ImGui::Begin()`이 `false`를 반환하면 창이 접혀 있어도 반드시 `ImGui::End()`를 호출해야 합니다.

```cpp
// 개선
if (ImGui::Begin("Hiearchy", nullptr))
{
    // 내용
}
ImGui::End();  // Begin 결과와 무관하게 항상 호출
```

### 5-3. `ImGuiCond_Appearing` vs `ImGuiCond_Always`

`Inspector`는 `ImGuiCond_Appearing`을 사용하지만 `Hiearchy`는 `SetNextWindowPos`에 조건 없이 매 프레임 강제 설정합니다. 일관된 방식을 선택해야 합니다.

### 5-4. `ImGui::GetIO()` 선언 후 미사용

```cpp
// Hiearchy.cpp ?
ImGuiIO& io = ImGui::GetIO();  // 선언만 하고 전혀 사용하지 않음
```

---

## 6. EditorTool 구조 개선

### 6-1. 권장 디렉터리 구조

```
EditorTool/
├── Core/
│   ├── EditorTool.h/.cpp          ? 진입점
│   ├── EditorToolManager.h/.cpp   ? 윈도우 레지스트리만
│   └── SelectionContext.h/.cpp    ? 선택 상태 (신규)
├── Windows/
│   ├── EditorWindow.h             ? 베이스
│   ├── Hierarchy.h/.cpp           ? (오타 수정)
│   ├── Inspector.h/.cpp
│   ├── SceneWindow.h/.cpp
│   ├── GameEditorWindow.h/.cpp
│   ├── Project.h/.cpp
│   ├── FolderContents.h/.cpp
│   ├── LogWindow.h/.cpp
│   └── MainMenuBar.h/.cpp
├── Scene/
│   ├── SceneCamera.h/.cpp
│   ├── SceneGrid.h/.cpp
│   └── Gizmo (Effects.h 분리)
├── Asset/
│   ├── AssetCache.h/.cpp          ? 파일 캐시 (신규)
│   ├── MeshThumbnail.h/.cpp
│   └── TextureManager.h/.cpp
└── Define.h
```

### 6-2. `EditorWindow` 베이스 클래스 개선

```cpp
// 현재 ? 윈도우 크기/위치를 Vec2로 저장하고 ImVec2로 매번 변환
ImVec2 GetEWinPos() { return ImVec2(_winPos.x, _winPos.y); }

// 개선 ? ImVec2로 직접 저장
class EditorWindow
{
protected:
    ImVec2 _winPos  = {0, 0};
    ImVec2 _winSize = {0, 0};
    bool   _isOpen  = true;      // 창 열림/닫힘 상태 추가
    string _windowName;          // 윈도우 이름 통일 관리
};
```

---

## 7. 기능 누락 / UX 개선

| 기능 | 현재 상태 | 개선 방안 |
|------|-----------|-----------|
| **Undo/Redo** | 없음 | `Command 패턴`으로 Transform 변경 등 히스토리 관리 |
| **오브젝트 삭제** | `ShortcutManager::DeleteObject()` 존재하나 Hierarchy UI에서 우클릭 메뉴 없음 | 우클릭 컨텍스트 메뉴 추가 |
| **다중 선택** | 단일 선택만 (`_selectedH` 하나) | `vector<int64>` 또는 `set<int64>`로 다중 선택 지원 |
| **씬 저장/불러오기** | `MainMenuBar::MenuFileList()` 존재 확인 필요 | XML 기반 씬 직렬화 완성 |
| **Hierarchy 부모-자식** | 평면 리스트 | `TreeNode`로 계층 구조 표현 |
| **인스펙터 컴포넌트 추가** | 없음 | "Add Component" 버튼 및 검색 팝업 |
| **씬 뷰 Gizmo 회전** | Translate/Scale만 확인 | Rotate Gizmo 동작 검증 필요 |
| **에디터 레이아웃 저장** | 없음 | `imgui.ini` 활용 또는 커스텀 INI 저장 |
| **로그 타임스탬프** | `LogMessage.time` 있으나 표시 여부 불명 | 로그 창에 시간 컬럼 추가 |

---

## 8. 우선순위 요약

### ?? 즉시 수정 (버그/크래시)
1. `Hiearchy.cpp` Selectable 쉼표 연산자 버그 → `isSelected` 분리
2. `GetEditorWindow()` 미반환 UB → `nullptr` 반환 추가
3. `wstring→string` 변환 한글 깨짐 → `Utils::ToString()` 통일

### ?? 단기 개선 (코드 품질)
4. 헤더 내 함수 구현 `.cpp`로 분리
5. `ImGui::Begin()` 반환값 처리 및 `End()` 보장
6. 오타 수정: `Hiearchy` → `Hierarchy`, `cashes` → `cached`
7. `ImGuiIO& io` 미사용 변수 제거
8. `Init()` 중복 생성 방어 코드 강화

### ?? 중기 개선 (구조)
9. `EditorToolManager` 책임 분리 (`SelectionContext`, `AssetCache`)
10. Preview 카메라/라이트 공유 (`PreviewSceneContext`)
11. 파일 캐시 `map` → `unordered_map`
12. `GetMetaType()` 룩업 테이블화
13. 썸네일 캐시 최대 크기 제한

### ?? 장기 개선 (기능)
14. Undo/Redo Command 패턴
15. Hierarchy TreeNode 계층 구조
16. 다중 오브젝트 선택
17. 에디터 레이아웃 INI 저장
18. "Add Component" UI
