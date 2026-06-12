#pragma once
#include "Renderer.h"
#include "ConstantBuffer.h"

// 씬 그리드 파라미터 (HLSL SceneGrid.hlsl GridParamsBuffer b8 과 일치)
struct GridParamsDesc
{
	float fadeStart = 40.f;  // 페이드 시작 거리 (m)
	float fadeEnd   = 90.f;  // 완전히 사라지는 거리 (m)
	float baseAlpha = 0.3f;  // 라인 기본 농도
	float padding   = 0.f;
};

// 에디터 씬 그리드 — editorInternal 오브젝트로 씬에 상주, Pass 3(Transparent)에서 렌더.
// 카메라 XZ 를 셀 단위로 스냅해 따라가므로 유한 그리드가 무한처럼 보인다.
class SceneGrid : public Renderer
{
	using Super = Renderer;

public:
	SceneGrid();
	virtual ~SceneGrid();

	// count×count 칸, 셀 크기 size(m) — fadeStart/End: 카메라 거리 페이드, alpha: 라인 농도
	void Init(int32 count, float size, float fadeStart, float fadeEnd, float alpha);
	virtual void Update() override;

	void Draw(const RenderContext& ctx) override;

	// 그리드 전체 범위 박스 — 기본(1m 고정 박스)이면 발밑 그리드가 절두체 컬링으로 사라진다
	void TransformBoundingBox() override;
	// 그리드끼리 인스턴싱 배칭 금지 (기본 (0,0) ID 는 서로 묶여 하나만 그려짐)
	InstanceID GetInstanceID() override { return make_pair((uint64)this, (uint64)0); }

private:
	shared_ptr<Geometry<VertexTextureData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	shared_ptr<class HlslShader> _shader;

	float _gridSize = 1.f;
	float _gridExtent = 100.f; // count * size * 0.5
	GridParamsDesc _params;
	shared_ptr<ConstantBuffer<GridParamsDesc>> _paramsCB;
};
