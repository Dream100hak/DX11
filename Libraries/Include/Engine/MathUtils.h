#pragma once
#include "Primitive3D.h"
class MathUtils
{
public:

	static float Random(float r1, float r2);
	static Vec2 RandomVec2(float r1, float r2);
	static Vec3 RandomVec3(float r1, float r2);
	static float Dot(const Vec4& v1, const Vec4& v2)
	{
		return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
	}

	static const float INF;
	static const float PI;
};

