<!-- 파일 인코딩: UTF-8 (BOM 없음) | 편집 시 반드시 UTF-8로 저장할 것 -->
<!-- VS Code: 우하단 인코딩 클릭 → 'UTF-8' 선택 | Visual Studio: 파일 → 고급 저장 옵션 → UTF-8 -->

# DX11 Engine ? 구조적 개선 보고서 (포트폴리오 / 학습 목적)

> **목적**: 포트폴리오 완성도와 그래픽스 엔지니어링 이해도를 높이기 위한 엔진 구조 개선 항목 정리  
> **기준**: 현재 코드베이스를 기반으로, 실무·면접에서 자주 언급되는 개념 위주로 구성  
> **저장소**: https://github.com/Dream100hak/DX11 (branch: main)

---

## ?? 현재 진행 상태 (작업 이어받기용)

> 이 섹션을 기준으로 어디서든 작업을 이어받을 수 있습니다.  
> 완료된 항목은 ?, 진행 중은 ??, 미착수는 ?로 표시합니다.

### 결정된 방향 (확정)

| 항목 | 결정 내용 |
|------|-----------|
| **셰이더 시스템** | FX11 전면 교체. 기존 `.fx`는 제거하고 네이티브 HLSL(`.hlsl`)로 전환 |
| **마이그레이션 방식** | 기존 `Shader` 클래스 유지 + `HlslShader` 병렬 추가 → 점진적 교체 |
| **우선 구현 순서** | HlslShader 래퍼 → Standard 셰이더 변환 → Deferred Rendering → Post-Process |
| **책 예제 셰이더** | `02~29번` `.fx` 파일들은 엔진에서 미사용 확인 → 정리 대상 |

### 실제 엔진에서 사용 중인 셰이더 목록

```
Engine 로드:
  - 01. Standard.fx       ← 메인 렌더링 (최우선 마이그레이션 대상)
  - 00. ShadowMap.fx      ← 섀도우 맵 생성
  - 00. Ssao.fx           ← SSAO
  - 00. SsaoBlur.fx       ← SSAO 블러
  - 00. SsaoNormalDepth.fx
  - 01. Sky.fx            ← 스카이박스
  - 01. Outline.fx        ← 오브젝트 아웃라인
  - 01. Terrain.fx        ← 지형
  - 01. Thumbnail.fx      ← 메시 프리뷰
  - 01. Collider.fx       ← 콜라이더 디버그

EditorTool 로드:
  - 01. CubeMap.fx        ← 큐브맵
  - 01. SceneGrid.fx      ← 에디터 그리드
  - 01. DebugTexture.fx   ← 텍스처 디버그
  - 01. Fire.fx           ← 파이어 파티클
  - 01. Rain.fx           ← 레인 파티클
```

### 다음 작업 순서 (여기서부터 시작)

```
✅ Step 1. Engine/HlslShader.h + HlslShader.cpp 생성
✅ Step 2. Engine/RenderStateManager.h + .cpp 생성
✅ Step 3. Shaders/HLSL/ 폴더 생성 + 공용 include 파일 작성
✅ Step 4. 01. Standard.fx → Standard_VS.hlsl + Standard_PS.hlsl 변환
✅ Step 5. 나머지 셰이더 동일 패턴으로 순차 변환
    (ShadowMap ✅ → Outline ✅ → Sky ✅ → Terrain FX 유지(HS/DS 미지원) → 나머지)
✅ Step 6. 기존 FX 파일들 제거 및 C++ 코드에서 FX11 의존성 제거
  (Material/SkyBox HlslShader 전환 완료, Terrain/SSAO FX 유지)
✅ Step 7. Frustum Culling 구현
         - Engine/Frustum.h + Frustum.cpp 추가
     - p-vertex 방식 AABB 판정 (6 평면)
 - Camera::SortGameObject() 에 _frustum.Update() + IsInFrustum() 연동
           - BoundingBox 미계산 오브젝트 자동 통과 처리
✅ Step 8. 렌더 큐 (Render Queue) 구현
      - Define.h 에 RenderQueue enum 추가 (Background/Opaque/AlphaTest/Transparent/Overlay)
   - Material 에 _renderQueue 멤버 추가 + Clone() 복사 포함
 - Camera::SortGameObject() 에서 _vecOpaque / _vecTransparent 벡터 분리
   - 불투명: Front-to-Back 정렬 (Early-Z 활용)
        - 투명:   Back-to-Front 정렬 (알파 블렌딩 정확도)
    - Camera::Render_Forward() 에서 Opaque → Transparent 순서로 각각 렌더
✅ Step 9. 렌더러 단일 진입점 (RenderContext) 구현
   - Engine/RenderContext.h 추가
           struct RenderContext { tech, view, proj, light, shaderOverride, buffer }
    - Renderer 기반 클래스에 virtual Draw(const RenderContext&) 단일 진입점 추가
    - MeshRenderer::Draw()   — HlslShader / FX11 분기, Single/Instanced 통합
      - ModelRenderer::Draw()— PushMeshes() 내부 헬퍼로 Single/Instanced 공통화
     - ModelAnimator::Draw()  — PushBufferInstancing() 호출
      - SceneGrid::Draw()      — DrawGrid() 호출
         - InstancingManager 를 Draw(RenderContext) 기반으로 전환
       - 레거시 Render / RenderInstancing / RenderThumbnail 전면 제거
        (Renderer / MeshRenderer / ModelRenderer / ModelAnimator / SceneGrid)
       - ModelRenderer::PushBuffer / PushBufferInstancing → PushMeshes() 로 통합 후 제거
    - MeshThumbnail::Draw() 도 RenderContext 기반으로 전환
✅ Step 10. InstancingManager 시그니처 정리
     - Render(const RenderContext& baseCtx, vector<shared_ptr<GameObject>>& gameObjects)
 - RenderStaticObject / RenderAnimRenderer → private
           - Camera::Render_Forward() 에서 RenderContext 구성 후 전달

✅ Step 11. UpdateTweenData() 위치 분리
           - ModelAnimator::Update() 에서 호출하도록 이동
    - InstancingManager 에서 UpdateTweenData() 호출 제거

✅ Step 12. RenderContext 에 HlslShader override 지원 추가
   - struct RenderContext 에 shared_ptr<HlslShader> hlslOverride 필드 추가 ✅
   - MeshRenderer::Draw() 에서 hlslOverride 분기 처리 ✅
 - Shadow / Outline 패스에서 HlslShader 기반 override 사용 가능하게 준비 ✅

✅ Step 13. 멀티 라이트 지원 (Phase 1 마무리)
   - Define.h 에 MAX_LIGHTS = 16 추가 ✅
   - BindShaderDesc.h 에 DirectionalLightData / LightArrayDesc 구조 추가 ✅
   - RenderContext 에 lightArray 필드 추가 ✅
   - Lighting.hlsli 에 ComputeDirectionalLightArray() 함수 추가 ✅
   - HlslShader 에 PushLightArrayData() 메서드 추가 ✅
   - Scene 에 GetLights() 메서드 추가 ✅
   - Camera::Render_Forward() 에서 모든 라이트 수집 및 배열 전달 ✅
   - MeshRenderer::Draw() 에서 lightArray 우선 처리 ✅
   - Common.hlsli 에 MAX_LIGHTS 상수 추가 ✅

→ Step 14. Deferred Rendering (Phase 2 시작)

```

### 현재 구조에 남아 있는 문제점 (해결됨)

```
✅ Issue 1. InstancingManager::Render() 시그니처 정리 완료
    - Render(const RenderContext& baseCtx, vector<GameObject>&) 로 통일

✅ Issue 2. InstancingManager::RenderStaticObject / RenderAnimRenderer 을 private으로 변경 완료
    - Render() 하나만 public API

✅ Issue 3. Camera::Render_Forward() 최적화
  - RenderContext 를 사전에 구성 후 전달 (씬 중복 조회 방지)

✅ Issue 4. ModelAnimator::UpdateTweenData() 위치 분리 완료
    - ModelAnimator::Update() 에서 호출
    - 렌더 루프에서 순수 Draw만 담당

✅ Issue 5. RenderContext.hlslOverride 지원 추가 완료
    - MeshRenderer::Draw() 에서 우선 처리 (hlslOverride → HlslShader → FX11)
    - Shadow / Outline 패스 준비 완료

