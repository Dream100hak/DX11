#pragma once

// -----------------------------------------------------------
// Frustum
//  - View-Projection 행렬에서 6개 평면(절두체)을 추출
//  - BoundingBox / BoundingSphere 컬링 판정
// -----------------------------------------------------------
class Frustum
{
public:
	// 매 프레임 VP 행렬로 갱신
	void Update(const Matrix& viewProj);

	bool IsInFrustum(const BoundingBox& box)     const;
	bool IsInFrustum(const BoundingSphere& sphere) const;

private:
	// Left, Right, Bottom, Top, Near, Far
	array<Plane, 6> _planes;
};
