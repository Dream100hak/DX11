#include "Renderer.h"
#include "GameObject.h"
#include "Transform.h"

using namespace DirectX;

void Renderer::TransformBoundingBox()
{
	// 기본 구현 — 트랜스폼 월드 위치 중심, 0.5 반경(1×1×1). 대형 커스텀 렌더러는 override.
	Vec3 c{ 0, 0, 0 };
	if (auto t = GetTransform()) c = t->GetPosition();
	_boundingBox.Center = c;
	_boundingBox.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
}

InstanceID Renderer::GetInstanceID()
{
	return InstanceID{ 0, 0 };
}
