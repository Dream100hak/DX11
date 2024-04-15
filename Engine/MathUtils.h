#pragma once
#include "Primitive3D.h"
class MathUtils
{
public:

	static float Random();
	static float Random(float r1, float r2);
	static Vec2 RandomVec2(float r1, float r2);
	static Vec3 RandomVec3(float r1, float r2);
	static float Dot(const Vec4& v1, const Vec4& v2)
	{
		return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
	}
	static Matrix InverseTranspose(CXMMATRIX M)
	{
		// Inverse-transpose is just applied to normals.  So zero out 
		// translation row so that it doesn't get into our inverse-transpose
		// calculation--we don't want the inverse-transpose of the translation.
		XMMATRIX A = M;
		A.r[3] = ::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		XMVECTOR det = ::XMMatrixDeterminant(A);
		return ::XMMatrixTranspose(XMMatrixInverse(&det, A));
	}

	static const float INF;
	static const float PI;
};

