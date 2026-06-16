#pragma once
#include "Common.h"

// DX11 Engine/GeometryHelper 이식(1차) — 절차적 메시 정점 생성 (Vtx: pos/nrm/col/uv/tan).
namespace GeometryHelper
{
	// 중심 원점, 한 변 size 의 큐브 (24정점/36인덱스, 면별 노멀)
	inline void CreateCube(vector<Vtx>& v, vector<uint32>& idx, float size = 1.f, Vec3 col = { 0.8f, 0.8f, 0.8f })
	{
		v.clear(); idx.clear();
		const float h = size * 0.5f;
		struct Face { Vec3 n, t; Vec3 p[4]; };
		const Vec3 X{ 1,0,0 }, Y{ 0,1,0 }, Z{ 0,0,1 };
		Face faces[6] = {
			{ {0,0,1},  X, {{-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}} }, // +Z
			{ {0,0,-1}, X, {{ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h}} }, // -Z
			{ {1,0,0},  Z, {{ h,-h, h},{ h,-h,-h},{ h, h,-h},{ h, h, h}} }, // +X
			{ {-1,0,0}, Z, {{-h,-h,-h},{-h,-h, h},{-h, h, h},{-h, h,-h}} }, // -X
			{ {0,1,0},  X, {{-h, h, h},{ h, h, h},{ h, h,-h},{-h, h,-h}} }, // +Y
			{ {0,-1,0}, X, {{-h,-h,-h},{ h,-h,-h},{ h,-h, h},{-h,-h, h}} }, // -Y
		};
		const Vec2 uv[4] = { {0,1},{1,1},{1,0},{0,0} };
		for (int f = 0; f < 6; ++f)
		{
			uint32 base = (uint32)v.size();
			for (int k = 0; k < 4; ++k)
				v.push_back({ faces[f].p[k], faces[f].n, col, uv[k], faces[f].t });
			idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
			idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
		}
	}

	// XZ 평면 쿼드 (한 변 size, 위 방향 +Y)
	inline void CreateQuad(vector<Vtx>& v, vector<uint32>& idx, float size = 1.f, Vec3 col = { 0.8f, 0.8f, 0.8f })
	{
		v.clear(); idx.clear();
		const float h = size * 0.5f;
		const Vec3 n{ 0,1,0 }, t{ 1,0,0 };
		v.push_back({ {-h,0, h}, n, col, {0,1}, t });
		v.push_back({ { h,0, h}, n, col, {1,1}, t });
		v.push_back({ { h,0,-h}, n, col, {1,0}, t });
		v.push_back({ {-h,0,-h}, n, col, {0,0}, t });
		idx = { 0,1,2, 0,2,3 };
	}

	// 평면 — CreateQuad 별칭 (의미 명확용)
	inline void CreatePlane(vector<Vtx>& v, vector<uint32>& idx, float size = 1.f, Vec3 col = { 0.8f, 0.8f, 0.8f })
	{
		CreateQuad(v, idx, size, col);
	}

	// UV 구체 (위경도 분할) — 중심 원점, 반지름 radius
	inline void CreateSphere(vector<Vtx>& v, vector<uint32>& idx, float radius = 0.5f, Vec3 col = { 0.8f, 0.8f, 0.8f }, int stacks = 16, int slices = 24)
	{
		v.clear(); idx.clear();
		const float PI = 3.14159265358979f;
		for (int i = 0; i <= stacks; ++i)
		{
			float phi = PI * float(i) / float(stacks);      // 0..PI (극→극)
			float y = cosf(phi), r = sinf(phi);
			for (int j = 0; j <= slices; ++j)
			{
				float theta = 2.f * PI * float(j) / float(slices);
				Vec3 n{ r * cosf(theta), y, r * sinf(theta) };
				Vec3 p{ n.x * radius, n.y * radius, n.z * radius };
				Vec3 t{ -sinf(theta), 0.f, cosf(theta) };
				Vec2 uv{ float(j) / float(slices), float(i) / float(stacks) };
				v.push_back({ p, n, col, uv, t });
			}
		}
		const int ring = slices + 1;
		for (int i = 0; i < stacks; ++i)
			for (int j = 0; j < slices; ++j)
			{
				uint32 a = i * ring + j, b = a + ring;
				idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
				idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
			}
	}
}
