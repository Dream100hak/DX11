#include "pch.h"
#include "Frustum.h"

void Frustum::Update(const Matrix& viewProj)
{
	// Gribb-Hartmann: VP 행렬 열에서 6 평면 추출 (row-major XMFLOAT4X4 전치 활용)
	Matrix m = viewProj.Transpose();

	// Left  : col3 + col0
	_planes[0] = Plane(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41);
	// Right : col3 - col0
	_planes[1] = Plane(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41);
	// Bottom: col3 + col1
	_planes[2] = Plane(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42);
	// Top   : col3 - col1
	_planes[3] = Plane(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42);
	// Near  : col2
	_planes[4] = Plane(m._13,         m._23,          m._33,         m._43);
	// Far   : col3 - col2
	_planes[5] = Plane(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43);

	for (auto& p : _planes)
		p.Normalize();
}

bool Frustum::IsInFrustum(const BoundingBox& box) const
{
	// 각 평면에 대해 AABB의 "가장 먼 꼭짓점(p-vertex)"이 뒷면이면 완전 밖(컬링)
	XMFLOAT3 c = box.Center;
	XMFLOAT3 e = box.Extents;

	for (const auto& p : _planes)
	{
		// p-vertex: 평면 법선 방향에서 가장 돌출된 꼭짓점
		float px = (p.x >= 0.f) ? (c.x + e.x) : (c.x - e.x);
		float py = (p.y >= 0.f) ? (c.y + e.y) : (c.y - e.y);
		float pz = (p.z >= 0.f) ? (c.z + e.z) : (c.z - e.z);

		// p-vertex 가 평면 뒷면이면 AABB 완전 밖
		if (p.x * px + p.y * py + p.z * pz + p.w < 0.f)
			return false;
	}
	return true;
}

bool Frustum::IsInFrustum(const BoundingSphere& sphere) const
{
	for (const auto& p : _planes)
	{
		float dist = p.x * sphere.Center.x
				   + p.y * sphere.Center.y
				 + p.z * sphere.Center.z + p.w;
		if (dist < -sphere.Radius)
			return false;
	}
	return true;
}
