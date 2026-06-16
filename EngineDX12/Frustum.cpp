#include "Frustum.h"

using namespace DirectX;

// row_major VP (m[row][col]) 에서 6평면 추출 (좌:row3+row0 ... ). Gribb/Hartmann.
void Frustum::Update(const Matrix& vp)
{
	const float (*m)[4] = vp.m;
	auto setPlane = [&](int i, float a, float b, float c, float d)
	{
		float len = sqrtf(a * a + b * b + c * c);
		if (len < 1e-8f) len = 1.f;
		_planes[i] = XMFLOAT4(a / len, b / len, c / len, d / len);
	};
	setPlane(0, m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // left
	setPlane(1, m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // right
	setPlane(2, m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // top
	setPlane(3, m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // bottom
	setPlane(4, m[0][2], m[1][2], m[2][2], m[3][2]);                                         // near
	setPlane(5, m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // far
}

bool Frustum::Contains(const BoundingBox& box) const
{
	for (int i = 0; i < 6; ++i)
	{
		const XMFLOAT4& p = _planes[i];
		// 박스에서 평면 법선 방향으로 가장 먼 점(positive vertex)이 평면 뒤면 완전히 바깥
		float r = box.Extents.x * fabsf(p.x) + box.Extents.y * fabsf(p.y) + box.Extents.z * fabsf(p.z);
		float d = p.x * box.Center.x + p.y * box.Center.y + p.z * box.Center.z + p.w;
		if (d < -r) return false;
	}
	return true;
}
