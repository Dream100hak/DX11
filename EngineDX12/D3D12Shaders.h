#pragma once
// 인라인 HLSL 셰이더 소스 (C++ 문자열 리터럴) — D3D12Shaders.cpp 에 정의.
// 모두 공용 SceneCB(b0) 레이아웃(kSceneCB)을 prepend 해 빌드. D3D12Device::CreatePipeline 이 CompileDxc 로 컴파일.
// (기존엔 D3D12Device.cpp 의 file-static 이었으나 파일 분리로 외부 linkage 로 승격)
#include <string>

extern const char* kSceneCB;          // 공용 상수버퍼 선언 (모든 셰이더 prepend)
extern const std::string kMeshShader;     // 메시/라이팅 (래스터 + RT GI 샘플)
extern const std::string kGatherShader;   // DDGI 프로브 gather (compute)
extern const std::string kSkyShader;      // 스카이박스 (절차적 하늘 / 큐브맵)
extern const std::string kGridShader;     // 씬 그리드
extern const std::string kOutlineShader;  // 선택 아웃라인
extern const std::string kParticleShader; // 파티클 빌보드
extern const std::string kTessShader;     // 테셀레이션 터레인
extern const std::string kWaterShader;    // 물 평면 (파도/프레넬)
extern const std::string kProbeViz;       // DDGI 프로브 시각화
