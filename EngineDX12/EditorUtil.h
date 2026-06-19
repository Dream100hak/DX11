#pragma once
// 에디터 공용 헬퍼 — D3D12Editor/Spawn/Serialize/Inspector 가 공유.
//   WToUtf8  : wstring → UTF-8 (ImGui 텍스트용)
//   BuildPrim: 프리미티브 종류 → 지오메트리 생성 (재생성/스폰 공용)
// (기존엔 D3D12Editor.cpp 의 file-static 이었으나 파일 분리로 외부 linkage 로 승격)
#include "Common.h"
#include "MeshRenderer.h"   // MeshPrim

std::string WToUtf8(const std::wstring& w);
void BuildPrim(MeshPrim prim, vector<Vtx>& v, vector<uint32>& idx);
// 직전 위젯 옆에 "(?)" 를 그리고 호버 시 설명 툴팁 (인스펙터 난해 파라미터 설명)
void HelpMarker(const char* desc);
// 색온도(Kelvin) → 정규화 RGB (라이트 색 — 1500K 촛불~12000K 한낮 그늘)
Vec3 KelvinToRGB(float kelvin);

#include <functional>
class Material;
// 유니티/언리얼식 머티리얼 슬롯 — 이름 표시 + 드롭(MAT_PATH) + Pick 팝업.
// 할당 시 로드된 공유 머티리얼로 onAssign 콜백. (MeshRenderer/ModelAnimator 공용)
void MaterialSlotGUI(const std::wstring& assetRoot, const std::shared_ptr<Material>& cur,
                     const std::function<void(std::shared_ptr<Material>)>& onAssign);
