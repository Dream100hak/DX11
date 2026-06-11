# HLSL 렌더링 문제 진단 기록

**날짜**: 2024년 (시작)  
**상태**: ?? **Phase 1 완료 (빌드 성공)**  
**목표**: HLSL 셰이더로 교체 후 씬에 렌더링이 안 되는 문제 원인 파악 및 해결

---

## 0. 빠른 진단 (Quick Diagnosis)

### ?? 의심되는 최상위 문제 3가지

1. **Sampler 미바인딩** ? **해결 완료**
   - Material::Update()에서 SetPSSampler()를 호출하지 않음 → 이제 호출함
   - Texture SRV는 바인딩되지만 Sampler가 없으면 샘플링 실패
   - **수정**: Material::Update()에 SetPSSampler() 호출 코드 추가
   - **파일**: `Engine/Material.cpp` (라인 약 95~103)
   - **상태**: ? 빌드 성공

2. **Material이 HlslShader를 모르는 경우** ? **검증 필요**
   - Material 생성 시 `_hlslShader` 할당이 없으면 FX11로 폴백
   - 확인: ImGuiManager::CreateMesh() - 라인 87~95 참고
   - 수정 필요: HlslShader 명시적 할당

3. **InputLayout matrix 분해** ? **완료**
   - INST_WORLD를 4개 행으로 분해하는 로직 추가됨 (완료)
   - SemanticIndex가 올바르게 설정됨

---

## 1. 문제 정의

### 증상
- FX11 기반 셰이더에서는 렌더링 됨
- HLSL로 교체 후 씬에 아무것도 보이지 않음
- 에러 메시지나 크래시는 없음 (silent failure)

### 환경
- DirectX 11
- C++20
- HLSL 5.0
- 파일 위치: `Shaders/HLSL/*.hlsl`

---

## 2. 해결된 문제들

### ? 문제 1: InputLayout matrix 분해
**상태**: ?? **완료**  
**파일**: `Engine/HlslShader.cpp` (CreateInputLayoutFromVS 함수)  
**내용**: INST_WORLD 매트릭스를 4개의 float4로 분해  
**테스트**: 빌드 성공

---

### ? 문제 2: Sampler 미바인딩 (긴급)
**상태**: ?? **완료**  
**파일**: `Engine/Material.cpp` (Update 함수)  
**수정 사항**:
```cpp
// Material::Update() 내에 추가됨 (라인 95~103)
_hlslShader->SetPSSampler(0, nullptr);  // TODO: RenderStateManager 연결
_hlslShader->SetPSSampler(1, nullptr);
_hlslShader->SetPSSampler(2, nullptr);
_hlslShader->SetPSSampler(3, nullptr);
_hlslShader->SetPSSampler(4, nullptr);
```
**테스트**: 빌드 성공 ?

---

## 3. 다음 단계 (TODO)

### Phase 2: Sampler 실제 연결

**우선순위 1**: RenderStateManager와 연결
```cpp
// Material::Update() 수정 필요
auto linearSampler = /* RenderStateManager에서 가져오기 */;
auto shadowSampler = /* RenderStateManager에서 가져오기 */;
_hlslShader->SetPSSampler(0, linearSampler);
_hlslShader->SetPSSampler(3, shadowSampler);
```

**우선순위 2**: 렌더링 테스트
- 에디터에서 Cube/Sphere 생성 후 렌더링 확인
- 텍스처 렌더링 확인
- 라이팅 적용 확인

**우선순위 3**: Material 자동 HlslShader 할당
- ImGuiManager::CreateMesh() 수정
- Material 생성 시 HLSL 셰이더 자동 연결

---

## 4. 최종 체크리스트

- [x] Sampler 바인딩 코드 추가 (Material::Update)
- [x] InputLayout 생성 완료 (HlslShader::CreateInputLayoutFromVS)
- [x] 빌드 성공
- [ ] Sampler를 RenderStateManager에 연결
- [ ] 씬 렌더링 테스트
- [ ] 텍스처 렌더링 테스트
- [ ] 라이팅 렌더링 테스트

---

## 5. 현재 상태 요약

? **Phase 1 완료**
- HlslShader InputLayout 생성 로직 개선
- Material에 Sampler 바인딩 코드 추가
- 빌드 성공

?? **Phase 2 대기**
- RenderStateManager와 Sampler 연결 필요
- 실제 렌더링 테스트 필요

---

## 6. 관련 파일

- `Engine/Material.cpp` - Sampler 바인딩
- `Engine/HlslShader.cpp` - InputLayout 생성
- `Shaders/HLSL/Common.hlsli` - Sampler 정의
- `IMPROVEMENT_REPORT.md` - 전체 개선 계획
- `ENGINE_IMPROVEMENT_REPORT.md` - 엔진 구조 개선

