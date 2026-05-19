# HLSL 렌더링 문제 - 해결 기록 (요약)

**작성일**: 2024년  
**상태**: ?? Phase 1 완료, 빌드 성공  
**다음**: Phase 2 - RenderStateManager 연결 필요

---

## ?? 문제 상황

**원인**: HLSL 셰이더로 교체 후 씬에 렌더링이 안 됨 (FX11과 HLSL의 동시 지원 구조 전환 중)

**증상**:
- FX11 셰이더: 렌더링 정상
- HLSL 셰이더: 렌더링 안 됨 (화면 검정색)
- 에러/크래시 없음 (silent failure)

---

## ?? 발견된 3가지 문제

### 1?? Sampler 미바인딩 ? **해결**

**문제**: 
- Material::Update()에서 Texture SRV는 바인딩하지만 Sampler를 설정하지 않음
- GPU가 텍스처를 샘플링할 수 없음 → 렌더링 실패

**해결**:
- `Engine/Material.cpp` (라인 95~103)에 SetPSSampler() 호출 추가
- 현재: nullptr로 임시 바인딩 (나중에 RenderStateManager와 연결 예정)

```cpp
// Material::Update() 내 HlslShader 섹션
_hlslShader->SetPSSampler(0, nullptr);  // DiffuseMap
_hlslShader->SetPSSampler(1, nullptr);  // SpecularMap
_hlslShader->SetPSSampler(2, nullptr);  // NormalMap
_hlslShader->SetPSSampler(3, nullptr);  // ShadowMap
_hlslShader->SetPSSampler(4, nullptr);  // SsaoMap
```

### 2?? InputLayout Matrix 분해 ? **완료**

**문제**:
- INST_WORLD (matrix) 시맨틱이 4개의 float4로 올바르게 분해되지 않음
- 인스턴싱 렌더링 실패 가능성

**해결**:
- `Engine/HlslShader.cpp::CreateInputLayoutFromVS()` 개선
- INST_WORLD를 감지하고 INST_WORLD_0/1/2/3로 4개 행 분해
- SemanticIndex 올바르게 설정 (0~3)

### 3?? Material HlslShader 할당 ? **검증 필요**

**문제**:
- Material 생성 시 `_hlslShader`가 할당되지 않을 수 있음
- 폴백하여 FX11 경로로 실행

**확인 위치**:
- `EditorTool/ImGuiManager.cpp::CreateMesh()` (라인 87~95)
- Material 생성 후 SetHlslShader() 호출 확인 필요

---

## ? 완료된 작업

| 항목 | 파일 | 상태 |
|------|------|------|
| Sampler 바인딩 추가 | `Engine/Material.cpp` | ? 완료 |
| InputLayout matrix 분해 | `Engine/HlslShader.cpp` | ? 완료 |
| 빌드 확인 | - | ? 성공 |

---

## ?? 남은 작업 (Phase 2)

### 우선순위 1: RenderStateManager 연결

```cpp
// Material::Update()에서 Sampler를 실제로 바인딩
// 현재: nullptr 사용 → 실제 Sampler 객체 바인딩 필요

// TODO:
auto linearSampler = GRAPHICS->GetLinearSamplerState();  // 방법 확인 필요
auto shadowSampler = GRAPHICS->GetShadowSamplerState();  // 방법 확인 필요

_hlslShader->SetPSSampler(0, linearSampler);  // DiffuseMap
_hlslShader->SetPSSampler(1, linearSampler);  // SpecularMap
_hlslShader->SetPSSampler(2, linearSampler);  // NormalMap
_hlslShader->SetPSSampler(3, shadowSampler);  // ShadowMap
_hlslShader->SetPSSampler(4, linearSampler);  // SsaoMap
```

### 우선순위 2: 렌더링 테스트

1. 에디터에서 간단한 Mesh (Cube/Sphere) 생성
2. 렌더링 확인
3. 텍스처 적용 확인
4. 라이팅 적용 확인

### 우선순위 3: Material 자동 할당

- ImGuiManager::CreateMesh()에서 HlslShader 자동 할당 확인
- 또는 Material 기본값으로 HlslShader 설정

---

## ?? 관련 파일 맵

```
Engine/
  ├── Material.cpp      ← Sampler 바인딩 추가 (라인 95~103)
  ├── Material.h            ← _hlslShader 멤버
  ├── HlslShader.cpp  ← InputLayout 생성 개선
  ├── HlslShader.h          ← SetPSSampler() 인터페이스
  └── MeshRenderer.cpp      ← Draw() 호출

EditorTool/
  └── ImGuiManager.cpp      ← CreateMesh() (라인 87~95)

Shaders/HLSL/
├── Common.hlsli← Sampler 정의 (s0~s4)
  ├── Standard_VS.hlsl      ← 버텍스 셰이더
  └── Standard_PS.hlsl      ← 픽셀 셰이더

진단/문서:
  ├── HLSL_RENDERING_DIAGNOSIS.md  ← 상세 진단 기록
  ├── IMPROVEMENT_REPORT.md        ← EditorTool 개선 보고
  └── ENGINE_IMPROVEMENT_REPORT.md ← 엔진 구조 개선 계획
```

---

## ?? 현재 상태

```
빌드 상태: ? 성공
렌더링: ? 테스트 필요
문서: ? 완료 (HLSL_RENDERING_DIAGNOSIS.md)

다음 단계: RenderStateManager 연결 → 렌더링 테스트
```

---

## ?? 참고 사항

### Sampler 임시 nullptr 사용 이유
- Graphics 클래스의 GetLinearSamplerState() 같은 메서드를 찾을 수 없음
- 나중에 RenderStateManager와 연결하여 실제 Sampler 객체 바인딩
- 우선 nullptr로 바인딩하여 빌드 성공 상태 유지

### HLSL 렌더링 파이프라인
```
MeshRenderer::Draw(RenderContext)
  ├─ Material 선택
  ├─ HlslShader 확인
  ├─ Bind() - 파이프라인에 셰이더 설정
  ├─ Push*Data() - 상수버퍼 전달
  │   ├─ PushGlobalData()     (b0: V, P, VP, VInv, Shadow, T)
  │   ├─ PushTransformData()  (b1: W)
  │   ├─ PushLightData()      (b2: Ambient, Diffuse, Specular, Direction)
  │   ├─ PushMaterialData()   (b3: Mat*, UseTexture, UseAlphaClip, etc)
  │   └─ PushBoneData()       (b4: Transforms[250])
  ├─ SetPSSRV()  - 텍스처 SRV 바인딩 (t0~t4)
  ├─ SetPSSampler() ← **추가됨** - 샘플러 바인딩 (s0~s4)
  ├─ VertexBuffer/IndexBuffer 바인딩
  └─ DrawIndexed() - 실제 드로우
```

---

## ?? 최종 목표

? **Phase 1**: HLSL 셰이더 기본 렌더링 파이프라인 수정 (완료)
?? **Phase 2**: RenderStateManager 연결 및 렌더링 확인 (예정)
?? **Phase 3**: Deferred Rendering 구현 (미래 계획)

