#include "pch.h"
#include "MathUtils.h"


const float MathUtils::INF = FLT_MAX;
const float MathUtils::PI = 3.1415926535f;

float MathUtils::Random()
{
	return  ((float)rand()) / (float)RAND_MAX;
}

float MathUtils::Random(float r1, float r2)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = r2 - r1;
	float val = random * diff;

	return r1 + val;
}

Vec2 MathUtils::RandomVec2(float r1, float r2)
{
	Vec2 result;
	result.x = Random(r1, r2);
	result.y = Random(r1, r2);

	return result;
}

Vec3 MathUtils::RandomVec3(float r1, float r2)
{
	Vec3 result;
	result.x = Random(r1, r2);
	result.y = Random(r1, r2);
	result.z = Random(r1, r2);

	return result;
}

uint16_t MathUtils::ConvertFloatToHalf(float value)
{
	uint32_t Result;

	auto IValue = reinterpret_cast<uint32_t*>(&value)[0];
	uint32_t Sign = (IValue & 0x80000000U) >> 16U;
	IValue = IValue & 0x7FFFFFFFU;      // Hack off the sign
	if (IValue >= 0x47800000 /*e+16*/)
	{
		// The number is too large to be represented as a half. Return infinity or NaN
		Result = 0x7C00U | ((IValue > 0x7F800000) ? (0x200 | ((IValue >> 13U) & 0x3FFU)) : 0U);
	}
	else if (IValue <= 0x33000000U /*e-25*/)
	{
		Result = 0;
	}
	else if (IValue < 0x38800000U /*e-14*/)
	{
		// The number is too small to be represented as a normalized half.
		// Convert it to a denormalized value.
		uint32_t Shift = 125U - (IValue >> 23U);
		IValue = 0x800000U | (IValue & 0x7FFFFFU);
		Result = IValue >> (Shift + 1);
		uint32_t s = (IValue & ((1U << Shift) - 1)) != 0;
		Result += (Result | s) & ((IValue >> Shift) & 1U);
	}
	else
	{
		// Rebias the exponent to represent the value as a normalized half.
		IValue += 0xC8000000U;
		Result = ((IValue + 0x0FFFU + ((IValue >> 13U) & 1U)) >> 13U) & 0x7FFFU;
	}
	return static_cast<uint16_t>(Result | Sign);
}

void MathUtils::ExtractFrustumPlanes(Vec4 planes[6], CXMMATRIX CM)
{
	Matrix M = CM;

	//
		// Left
		//
	planes[0].x = M(0, 3) + M(0, 0);
	planes[0].y = M(1, 3) + M(1, 0);
	planes[0].z = M(2, 3) + M(2, 0);
	planes[0].w = M(3, 3) + M(3, 0);

	//
	// Right
	//
	planes[1].x = M(0, 3) - M(0, 0);
	planes[1].y = M(1, 3) - M(1, 0);
	planes[1].z = M(2, 3) - M(2, 0);
	planes[1].w = M(3, 3) - M(3, 0);

	//
	// Bottom
	//
	planes[2].x = M(0, 3) + M(0, 1);
	planes[2].y = M(1, 3) + M(1, 1);
	planes[2].z = M(2, 3) + M(2, 1);
	planes[2].w = M(3, 3) + M(3, 1);

	//
	// Top
	//
	planes[3].x = M(0, 3) - M(0, 1);
	planes[3].y = M(1, 3) - M(1, 1);
	planes[3].z = M(2, 3) - M(2, 1);
	planes[3].w = M(3, 3) - M(3, 1);

	//
	// Near
	//
	planes[4].x = M(0, 2);
	planes[4].y = M(1, 2);
	planes[4].z = M(2, 2);
	planes[4].w = M(3, 2);

	//
	// Far
	//
	planes[5].x = M(0, 3) - M(0, 2);
	planes[5].y = M(1, 3) - M(1, 2);
	planes[5].z = M(2, 3) - M(2, 2);
	planes[5].w = M(3, 3) - M(3, 2);

	// Normalize the plane equations.
	for (int i = 0; i < 6; ++i)
	{
		XMVECTOR v = ::XMPlaneNormalize(::XMLoadFloat4(&planes[i]));
		::XMStoreFloat4(&planes[i], v);
	}
}
