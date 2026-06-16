#pragma once
#include "Renderer.h"

// DX11 Engine/ModelAnimator 이식(1차) — 스키닝 모델 렌더러. 현재 스키닝 드로우는 ModelRenderer 가
// 담당(전환 단계 통합). 추후 본 행렬 텍스처/트윈을 이 컴포넌트로 분리.
class ModelAnimator : public Renderer
{
public:
	ModelAnimator() : Renderer(RendererType::Animator) {}
};
