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
? Step 1. Engine/HlslShader.h + HlslShader.cpp 생성
           - VS / PS / GS / CS 로드
           - CB 슬롯 기반 바인딩 (SetCB / SetSRV / SetSampler)
           - BlendState / RasterizerState / DSS C++ 제어
           - Draw / DrawIndexed / DrawIndexedInstanced / Dispatch

? Step 2. Engine/RenderStateManager.h + .cpp 생성
           - 자주 쓰는 BS/RS/DSS 사전 생성 및 이름으로 조회
           - Global.fx의 SamplerState / BlendState / RasterizerState 이전

? Step 3. Shaders/HLSL/ 폴더 생성 + 공용 include 파일 작성
           - Common.hlsli   (CB 레이아웃, 구조체 정의)
           - Lighting.hlsli (ComputeDirectionalLight 등)
           - Shadow.hlsli   (CalcShadowFactor)

? Step 4. 01. Standard.fx → Standard_VS.hlsl + Standard_PS.hlsl 변환
           - MeshRenderer / ModelRenderer / ModelAnimator 버텍스 분리
           - Material SRV 바인딩을 슬롯 기반으로 교체
           - HlslShader 사용하도록 MeshRenderer 수정

? Step 5. 나머지 셰이더 동일 패턴으로 순차 변환
           (ShadowMap → Outline → Sky → Terrain → 나머지)

? Step 6. Deferred Rendering 구현 (HlslShader 기반)
           - GBuffer 클래스 (MRT RTV + SRV)
           - GBuffer_VS.hlsl + GBuffer_PS.hlsl
           - DeferredLighting_PS.hlsl
           - Graphics에 MRT 지원 추가
```

### 환경 세팅 (회사 PC에서 시작 전 확인)

```bash
# 1. 저장소 클론 (최초 1회)
git clone https://github.com/Dream100hak/DX11.git

# 2. 최신 상태 동기화
git pull origin main

# 3. 솔루션 열기
# DX11/DX11.sln 또는 EditorTool.vcxproj / Engine.vcxproj

# 4. 필요 라이브러리 확인
# Libraries/ 폴더 내 Engine.lib, FX11, DirectXTex 등 포함 여부 확인
# 없으면 집 PC의 Libraries/ 폴더도 커밋에 포함시켜야 함
```

---

---

## 목차
1. [렌더링 파이프라인 구조](#1-렌더링-파이프라인-구조)
2. [라이팅 시스템](#2-라이팅-시스템)
3. [렌더 큐 / 렌더 패스](#3-렌더-큐--렌더-패스)
4. [후처리 (Post-Processing)](#4-후처리-post-processing)
5. [씬 최적화 구조](#5-씬-최적화-구조)
6. [컴포넌트 시스템 확장](#6-컴포넌트-시스템-확장)
7. [리소스 관리 구조](#7-리소스-관리-구조)
8. [셰이더 시스템](#8-셰이더-시스템)
9. [물리 / 콜리전](#9-물리--콜리전)
10. [우선순위 로드맵](#10-우선순위-로드맵)

---

## 1. 렌더링 파이프라인 구조

### 현재 상태

```
Game::Update()
  └─ Scene::Render()
       └─ Camera::Render_Forward()
            └─ InstancingManager::Render()
                 ├─ RenderStaticObject()   (MeshRenderer / ModelRenderer)
                 └─ RenderAnimRenderer()   (ModelAnimator)
```

- **포워드 렌더링(Forward Rendering)만 존재**
- `PostGraphics` 클래스가 선언만 되어 있고 **비어 있음** (`class PostGraphics {};`)
- PreRender / Render / PostRender JobQueue 구조는 있지만 **PostRender에 실질적인 패스가 없음**
- 씬에 라이트가 여러 개 있어도 `GetLight()`가 첫 번째 하나만 반환 → **멀티라이트 미지원**

---

### 1-1. ? 디퍼드 렌더링 (Deferred Rendering) 추가 ? 최우선

포트폴리오에서 가장 임팩트가 큰 항목입니다.

**G-Buffer 구성 (최소)**

| 버퍼 | 포맷 | 내용 |
|------|------|------|
| RT0 | `R8G8B8A8_UNORM` | Albedo (diffuse) |
| RT1 | `R16G16B16A16_FLOAT` | World Normal |
| RT2 | `R8G8B8A8_UNORM` | Metallic / Roughness / AO |
| RT3 | `R32_FLOAT` or DSV | Depth |

**구현 흐름**

```
[Geometry Pass]   각 오브젝트 → G-Buffer MRT에 기록
[Lighting Pass]   G-Buffer를 읽어 화면 공간에서 라이팅 계산
[Forward Pass]    투명 오브젝트, 파티클 → 별도 포워드 패스
[Post Pass]       Bloom, Tone Mapping 등
```

**추가해야 할 클래스**

```cpp
class GBuffer               // MRT RTV + SRV 관리
class DeferredLightingPass  // 화면 전체 쿼드에 라이팅 셰이더
class RenderPipeline        // Forward / Deferred 선택 가능하게 추상화
```

**왜 중요한가**  
- 디퍼드는 라이트 수에 상관없이 O(픽셀) 비용으로 라이팅 가능 (포워드는 O(픽셀 × 라이트))  
- 포인트 라이트 수십 개 배치가 가능해짐  
- 현재 구조에서 `Graphics`가 단일 RTV만 갖고 있어 MRT 지원 추가 필요

---

### 1-2. 렌더링 파이프라인 추상화

현재 `Camera::Render_Forward()`가 `InstancingManager`를 직접 호출하고,  
`Game::Update()` 내부에서 씬/GUI 순서가 뒤섞여 있습니다.

```cpp
// 현재 ? Game::Update() 에 렌더 로직이 뒤섞임
GRAPHICS->RenderBegin();
SCENE->Update();        // 업데이트인지 렌더인지 불명확
GUI->Update();
ImGui::Begin("Scene");
_gameDesc.app->Render();
GUI->Render();
GRAPHICS->PostRenderBegin();
```

**개선 ? 렌더 패스를 명시적으로 분리**

```cpp
// 권장 구조
void RenderPipeline::Execute()
{
    ShadowPass();       // Depth-only, light POV
    GBufferPass();      // Geometry → G-Buffer (deferred) or Forward
    LightingPass();     // Deferred lighting
    ForwardPass();      // Transparent, particles
    PostProcessPass();  // Bloom, SSAO, Tone mapping
    UIPass();           // ImGui / HUD
}
```

---

## 2. 라이팅 시스템

### 현재 상태
- **Directional Light 1개만** 실질적으로 동작 (`Scene::GetLight()`가 단일 반환)
- `Light::S_MatView`, `S_MatProjection`, `S_Shadow` 가 **static** 변수 → 라이트 하나 가정
- 셰이더의 `ComputeDirectionalLight()` 루프가 있지만 실제로는 1개만 처리

```cpp
// Light.h ? static으로 전역화 되어 있어 멀티라이트 불가
static Matrix S_MatView;
static Matrix S_MatProjection;
static Matrix S_Shadow;
```

### 2-1. ? 멀티 라이트 지원

```cpp
// 개선 ? LightDesc 배열로 확장
#define MAX_LIGHTS 16

struct LightBufferDesc
{
    LightDesc lights[MAX_LIGHTS];
    int32 lightCount;
    Vec3 padding;
};
```

씬의 `_lights` 컨테이너를 셰이더에 일괄 전달하는 구조로 변경.  
디퍼드와 조합하면 수십 개의 라이트도 성능 부담 없이 표현 가능.

### 2-2. ? Point Light / Spot Light 추가

현재 `LightType` 열거형에 `Directional` 외 타입이 있으나 셰이더 구현이 없습니다.

```cpp
enum LightType { Directional, Point, Spot };

// Light.h 에 이미 선언되어 있으나 셰이더에서 Point/Spot 분기 없음
```

**구현 방향**
- `LightDesc`에 `position`, `range`, `spotAngle` 추가
- 셰이더에서 `lightType`에 따라 감쇠(Attenuation) 계산 분기
- 디퍼드 렌더링과 조합 시 → **Light Volume** (구/콘 메시로 영향 범위 렌더)

### 2-3. PBR (Physically Based Rendering) ? 중장기

현재 라이팅은 고전적인 Phong/Blinn-Phong 모델입니다.  
`MaterialDesc`에 `metallic`, `roughness`가 없습니다.

```
현재: ambient + diffuse(Lambert) + specular(Phong)
목표: Cook-Torrance BRDF (Metallic-Roughness 워크플로우)
      - NDF: GGX Trowbridge-Reitz
      - Geometry: Smith-Schlick
      - Fresnel: Schlick approximation
      - IBL (Image Based Lighting): 환경맵 반사
```

**MaterialDesc 확장**
```cpp
struct MaterialDesc
{
    // 기존
    Color diffuse;
    Color specular;
    // 추가
    float metallic  = 0.f;
    float roughness = 0.5f;
    float ao        = 1.f;
    float padding;
    int32 useMetallicMap  = 0;
    int32 useRoughnessMap = 0;
    int32 useEmissiveMap  = 0;
};
```

---

## 3. 렌더 큐 / 렌더 패스

### 현재 상태
- `JobQueue`(PreRender / Render / PostRender) 구조가 있지만,  
  **오브젝트 정렬(Sort)이 없고** 렌더 순서가 삽입 순서에 의존합니다.
- `Camera::_vecForward`에 모든 오브젝트를 담아 한 번에 렌더합니다.
- 투명 오브젝트와 불투명 오브젝트의 **구분이 없습니다**.

### 3-1. ? 렌더 큐 (Render Queue) 도입

Unity 방식으로 렌더 큐 값을 기준으로 정렬:

```cpp
enum class RenderQueue : int32
{
    Background  = 1000,  // 스카이박스
    Opaque      = 2000,  // 불투명 (Front-to-Back 정렬 → Early-Z 활용)
    AlphaTest   = 2450,  // 알파 클립
    Transparent = 3000,  // 투명 (Back-to-Front 정렬 필수)
    Overlay     = 4000,  // UI, 이펙트
};
```

```cpp
// Material 또는 Renderer에 큐 값 추가
class Renderer
{
    RenderQueue _renderQueue = RenderQueue::Opaque;
};

// Camera::SortGameObject() 개선
// 불투명: 카메라에서 가까운 순 (Front-to-Back) → Early-Z 통과율 향상
// 투명:   카메라에서 먼 순  (Back-to-Front)  → 알파 블렌딩 정확도
```

현재 `_vecForward`는 단순 삽입 순서이므로 **투명 오브젝트가 불투명 앞에 그려지면 배경이 비침**.

### 3-2. 렌더 패스 분리 구조

```cpp
class RenderPass
{
public:
    virtual void Setup()   = 0;  // RTV/DSV 바인딩
    virtual void Execute() = 0;  // DrawCall
    virtual void Cleanup() = 0;  // 언바인딩

    int32 _priority = 0;         // 패스 실행 순서
    bool  _enabled  = true;
};

// 패스 예시
class ShadowMapPass    : public RenderPass { ... };
class OpaquePass       : public RenderPass { ... };
class TransparentPass  : public RenderPass { ... };
class SsaoPass         : public RenderPass { ... };
class PostProcessPass  : public RenderPass { ... };
```

---

## 4. 후처리 (Post-Processing)

### 현재 상태
- `PostGraphics` 클래스가 **비어 있음**
- SSAO 코드가 `EditorTool`에 있고 엔진 코어에 없음
- 셰이더 파일은 있음 (`00. Ssao.fx`, `00. SsaoBlur.fx`)

### 4-1. ? Post-Process 파이프라인 구축

```cpp
// PostGraphics를 실제 구현체로 채우기
class PostGraphics
{
    DECLARE_SINGLE(PostGraphics);
public:
    void Init();
    void Render();   // 전체 포스트 프로세스 체인 실행

    void AddEffect(shared_ptr<PostEffect> effect);

private:
    // 핑퐁 버퍼 ? 여러 패스를 체이닝할 때 사용
    shared_ptr<Texture> _pingBuffer;
    shared_ptr<Texture> _pongBuffer;

    vector<shared_ptr<PostEffect>> _effects;  // 순서대로 적용
};
```

### 4-2. 구현 목록 (우선순위 순)

| 효과 | 난이도 | 포트폴리오 임팩트 | 현재 |
|------|--------|-----------------|------|
| **Tone Mapping** (Reinhard / ACES) | ★☆☆ | 중 | ? |
| **Gamma Correction** | ★☆☆ | 중 | ? (LDR 파이프라인) |
| **SSAO** | ★★☆ | 높음 | 셰이더만 있음 |
| **Bloom** | ★★☆ | 높음 | ? |
| **Depth of Field** | ★★★ | 높음 | ? |
| **Motion Blur** | ★★★ | 중 | ? |
| **FXAA / TAA** | ★★★ | 높음 | ? |

**Tone Mapping 이 먼저인 이유**  
현재 파이프라인은 LDR(`R8G8B8A8_UNORM`)이라 값이 1을 넘으면 잘립니다.  
PBR / HDR 환경맵 추가 전 반드시 HDR 버퍼(`R16G16B16A16_FLOAT`) + Tone Mapping 도입 필요.

---

## 5. 씬 최적화 구조

### 현재 상태
- `Scene::_objects`가 `unordered_set` ? 전체 순회
- `Camera::SortGameObject()`가 **매 프레임 전체 오브젝트를 순회**하며 렌더러 유무 체크
- **Frustum Culling 없음** ? 카메라 밖의 오브젝트도 모두 InstancingManager로 전달
- `BVH` 클래스가 있지만 **씬 전체 컬링에 사용되지 않고** ModelMesh 레이캐스팅에만 사용

### 5-1. ? Frustum Culling

```cpp
class Frustum
{
public:
    void Update(const Matrix& viewProj);           // 매 프레임 절두체 갱신
    bool IsInFrustum(const BoundingBox& box) const;
    bool IsInFrustum(const BoundingSphere& sphere) const;

private:
    array<Plane, 6> _planes;  // Left, Right, Top, Bottom, Near, Far
};

// Camera::SortGameObject() 개선
void Camera::SortGameObject()
{
    _frustum.Update(_matView * _matProjection);

    for (auto& go : scene->GetObjects())
    {
        if (!_frustum.IsInFrustum(go->GetRenderer()->GetBoundingBox()))
            continue;     // ← 컬링
        _vecForward.push_back(go);
    }
}
```

현재 `Renderer::_boundingBox`가 이미 선언되어 있고 `TransformBoundingBox()`도 있으므로 Frustum 클래스만 추가하면 바로 연결 가능합니다.

### 5-2. 씬 공간 분할 (Spatial Partitioning)

| 방식 | 적합한 씬 | 구현 난이도 |
|------|-----------|------------|
| **Octree** | 정적 오브젝트 많은 씬 | ★★☆ |
| **BVH** | 애니메이션 포함 동적 씬 | ★★★ |
| **Grid** | 오픈 월드, 균일 분포 | ★☆☆ |

현재 `BVH` 클래스가 존재하므로 씬 레벨 BVH로 확장하는 것이 자연스럽습니다.

### 5-3. 오브젝트 캐시 분리

```cpp
// 현재 ? SortGameObject()가 매 프레임 전체 unordered_set 순회
unordered_set<shared_ptr<GameObject>> _objects;  // Scene

// 개선 ? 렌더 가능한 오브젝트만 별도 캐시
unordered_set<shared_ptr<GameObject>> _renderables;  // Renderer 있는 오브젝트만
unordered_set<shared_ptr<GameObject>> _cameras;      // 이미 있음 ?
unordered_set<shared_ptr<GameObject>> _lights;       // 이미 있음 ?
```

`Add()`/`Remove()` 시 `_renderables`도 동시에 갱신하면 SortGameObject에서 순회 대상이 줄어듭니다.

---

## 6. 컴포넌트 시스템 확장

### 현재 상태
- `ComponentType` 열거형이 고정 배열(`array<Component, FIXED_COMPONENT_COUNT>`)로 관리
- 컴포넌트 추가 시 **열거형과 배열 크기를 동시에 수정**해야 함 → 확장성 낮음
- `Renderer`가 ComponentType으로 등록되지 않고 별도 인덱스 존재
- `MonoBehaviour`는 `_scripts` 벡터로 분리 관리 ? 그나마 유연

### 6-1. 컴포넌트 탐색 개선

```cpp
// 현재 ? GetComponent<T>()가 전체 배열 + 스크립트 순회 (O(n))
// 개선 ? type_index 해시맵으로 O(1) 탐색
unordered_map<type_index, shared_ptr<Component>> _componentMap;

template<typename T>
shared_ptr<T> GetComponent()
{
    auto it = _componentMap.find(typeid(T));
    if (it != _componentMap.end())
        return static_pointer_cast<T>(it->second);
    return nullptr;
}
```

### 6-2. Transform 계층 구조 (Parent-Child)

현재 `Transform`에 **부모-자식 관계가 없습니다**.  
캐릭터 무기 어태치, UI 레이아웃, 씬 그래프 구현의 필수 요소입니다.

```cpp
class Transform : public Component
{
    // 추가 필요
    weak_ptr<Transform>         _parent;
    vector<shared_ptr<Transform>> _children;

public:
    void SetParent(shared_ptr<Transform> parent);
    void AddChild(shared_ptr<Transform> child);

    Matrix GetWorldMatrix() const;  // 부모 행렬 * 로컬 행렬 재귀 계산
    Matrix GetLocalMatrix() const;
};
```

현재 `GetWorldMatrix()`가 로컬 TRS만 계산하고 있어 계층 구조 반영이 안 됩니다.

### 6-3. 이벤트 / 메시지 시스템

```cpp
// 오브젝트 간 통신 구조 ? 현재 없음
// 최소 구현: 간단한 이벤트 버스
class EventBus
{
public:
    template<typename EventT>
    void Subscribe(function<void(const EventT&)> handler);

    template<typename EventT>
    void Publish(const EventT& event);
};

// 예: 충돌 이벤트, 데미지 이벤트 등
struct CollisionEvent { shared_ptr<GameObject> a, b; };
```

---

## 7. 리소스 관리 구조

### 현재 상태
- `ResourceManager`가 `unordered_map<wstring, shared_ptr<ResourceBase>>`로 단일 관리
- **비동기 로딩 없음** ? 대용량 모델 로딩 시 프레임 드랍
- `JobQueue`가 있지만 리소스 로딩에는 활용되지 않음
- 셰이더가 `FX11`(Effect 프레임워크) 기반 ? DX11 네이티브 셰이더 코드가 아님

### 7-1. 비동기 리소스 로딩

```cpp
// JobQueue를 활용한 비동기 로딩
class ResourceManager
{
    void LoadAsync(const wstring& key, const wstring& path,
                   function<void(shared_ptr<ResourceBase>)> onComplete);

    // 내부적으로 std::async 또는 스레드 풀 + JobQueue로 메인 스레드 콜백
};
```

### 7-2. 참조 카운트 기반 언로드

현재 `ResourceManager`에서 Add된 리소스는 **명시적 삭제 전까지 해제되지 않습니다**.

```cpp
// 개선 ? weak_ptr 캐시 + 실제 소유권은 사용처
unordered_map<wstring, weak_ptr<ResourceBase>> _cache;

shared_ptr<ResourceBase> Get(const wstring& key)
{
    auto it = _cache.find(key);
    if (it != _cache.end())
        if (auto res = it->second.lock()) return res;  // 살아있으면 반환

    // 없으면 로드 후 캐시
    auto loaded = Load(key);
    _cache[key] = loaded;
    return loaded;
}
// 마지막 사용처가 해제되면 자동으로 메모리 반환
```

---

## 8. 셰이더 시스템

### 현재 상태
- **FX11(Effect Framework)** 기반 ? `ID3DX11Effect`, `ID3DX11EffectPass` 사용
- 모든 셰이더 파라미터를 Effect Variable 이름 문자열로 접근 (`GetVariable("name")`)
- Technique/Pass 개념으로 셰이더 변형을 관리

### 8-1. FX11의 한계와 개선 방향

| 항목 | FX11 현재 | 권장 방향 |
|------|-----------|-----------|
| 컴파일 | 런타임 HLSL 컴파일 | 오프라인 사전 컴파일 (`.cso`) |
| 파라미터 접근 | 문자열 해시 룩업 | CB 슬롯 직접 바인딩 |
| 유지보수 | FX11 라이브러리 의존 | 네이티브 DX11 셰이더로 전환 |
| Compute Shader | 지원은 하나 어색함 | 네이티브로 간결하게 사용 가능 |

**현실적 제안**: FX11 전면 교체보다는 **새로 추가하는 PBR/Deferred 셰이더는 네이티브 DX11로 작성**, 기존 셰이더는 유지.

### 8-2. Shader Permutation (셰이더 변형 관리)

```cpp
// 현재 ? Technique 인덱스 상수가 #define으로 흩어져 있음
#define TECH_NORMAL    0
#define TECH_OUTLINE   1
#define TECH_WIREFRAME 3

// 개선 ? 셰이더 변형을 플래그 기반으로 관리
enum class ShaderFeature : uint32
{
    None          = 0,
    NormalMap     = 1 << 0,
    Skinned       = 1 << 1,
    Instanced     = 1 << 2,
    ReceiveShadow = 1 << 3,
    Wireframe     = 1 << 4,
};

class ShaderVariant
{
    ShaderFeature _features;
    // features 조합에 맞는 컴파일된 셰이더 반환
};
```

---

## 9. 물리 / 콜리전

### 현재 상태
- `AABB`, `OBB`, `Sphere` 콜라이더 구현 있음
- `Scene::CheckCollision()`에서 **O(n²) Brute Force** 충돌 검사
- `BVH`가 있지만 씬 레벨 Broad Phase에 사용되지 않음
- 물리 시뮬레이션(중력, 반발력 등) 없음

### 9-1. Broad Phase / Narrow Phase 분리

```
[Broad Phase]  BVH or Grid로 충돌 가능성 있는 쌍만 추려냄  O(n log n)
[Narrow Phase] AABB/OBB/Sphere 정밀 검사                  O(k), k << n²
```

현재 `BVH` 클래스를 씬 레벨 Broad Phase로 확장하면 성능이 크게 개선됩니다.

### 9-2. 물리 연동 (선택)

직접 구현보다는 `Bullet Physics` 또는 `PhysX` 연동이 현실적입니다.  
단순 중력 + 지형 충돌 정도는 직접 구현으로도 충분합니다.

---

## 10. 우선순위 로드맵

```
Phase 1 ? 핵심 렌더링 개선 (3~5주)
  ├─ [1] Frustum Culling 추가              ← BoundingBox 이미 있어 빠른 구현 가능
  ├─ [2] 렌더 큐 (불투명/투명 정렬)        ← RenderQueue enum + SortGameObject 개선
  ├─ [3] 멀티 라이트 지원                  ← LightDesc 배열 + 셰이더 수정
  └─ [4] Point / Spot Light 셰이더 구현

Phase 2 ? 디퍼드 렌더링 (4~6주)
  ├─ [5] G-Buffer (MRT) 구축              ← Graphics에 MRT RTV 추가
  ├─ [6] Geometry Pass 셰이더             ← 새 .fx 파일
  ├─ [7] Lighting Pass (Deferred)         ← 풀스크린 쿼드 셰이더
  └─ [8] Forward+ 로의 확장 고려

Phase 3 ? 후처리 & HDR (3~4주)
  ├─ [9]  HDR 버퍼 전환 (R16G16B16A16)
  ├─ [10] Tone Mapping (ACES)
  ├─ [11] SSAO (셰이더는 이미 있음)
  └─ [12] Bloom (Dual Kawase Blur)

Phase 4 ? PBR & IBL (4~6주)
  ├─ [13] MaterialDesc 확장 (metallic/roughness)
  ├─ [14] Cook-Torrance BRDF 셰이더
  ├─ [15] Irradiance Map (Diffuse IBL)
  └─ [16] Pre-filtered Environment Map (Specular IBL)

Phase 5 ? 구조 개선 (병행 가능)
  ├─ [17] Transform 부모-자식 계층 구조
  ├─ [18] RenderPass 추상화
  ├─ [19] 비동기 리소스 로딩
  └─ [20] 씬 공간 분할 (BVH 확장)
```

---

## 현재 엔진의 강점 (이미 잘 된 것들)

| 항목 | 평가 |
|------|------|
| **GPU Instancing** | ? `InstancingManager`로 MeshRenderer/Animator 모두 지원 |
| **Shadow Map** (PCF 9tap) | ? 셰이더 구현 완성도 좋음 |
| **Skinned Animation + Instancing** | ? `TweenDesc`로 블렌딩까지 지원 |
| **JobQueue** | ? 비동기 구조의 기반은 있음 |
| **Terrain** (Tessellation) | ? `DrawTess` 지원 |
| **BVH** (레이캐스팅용) | ? 기반 코드 있음, 확장 가능 |
| **Compute Shader** | ? `Dispatch` 지원, 파티클 SO 활용 |

---

> 면접/포트폴리오 관점에서 가장 임팩트가 큰 순서: **Deferred Rendering > PBR > Post-Process (HDR + Bloom + SSAO) > Frustum Culling > 멀티 라이트**
