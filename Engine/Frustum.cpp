#include "pch.h"
#include "Frustum.h"

void Frustum::Update(const Matrix& viewProj)
{
	// Gribb-Hartmann: row-vector convention (v*M) -> extract from columns directly
	const Matrix& m = viewProj;

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
	// 占쏙옙 占쏙옙涌?占쏙옙占쏙옙 AABB占쏙옙 "占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占쏙옙(p-vertex)"占쏙옙 占쌨몌옙占싱몌옙 占쏙옙占쏙옙 占쏙옙(占시몌옙)
	XMFLOAT3 c = box.Center;
	XMFLOAT3 e = box.Extents;

	for (const auto& p : _planes)
	{
		// p-vertex: 占쏙옙占?占쏙옙占쏙옙 占쏙옙占썩에占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占?占쏙옙占쏙옙占쏙옙
		float px = (p.x >= 0.f) ? (c.x + e.x) : (c.x - e.x);
		float py = (p.y >= 0.f) ? (c.y + e.y) : (c.y - e.y);
		float pz = (p.z >= 0.f) ? (c.z + e.z) : (c.z - e.z);

		// p-vertex 占쏙옙 占쏙옙占?占쌨몌옙占싱몌옙 AABB 占쏙옙占쏙옙 占쏙옙
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
