#pragma once
#include "Common.h"
#include <DirectXCollision.h>

// DX11 Engine/Frustum 이식 — 뷰프로젝션 → 6평면. AABB 컬링 테스트.
class Frustum
{
public:
	void Update(const Matrix& viewProj);              // row_major VP 에서 6평면 추출
	bool Contains(const DirectX::BoundingBox& box) const; // 박스가 절두체와 교차/포함이면 true

private:
	DirectX::XMFLOAT4 _planes[6]{}; // (nx,ny,nz,d), 정규화
};
