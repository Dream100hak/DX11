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

	// 원기둥 — 중심 원점, 반지름 r, 높이 h (옆면 + 위/아래 캡)
	inline void CreateCylinder(vector<Vtx>& v, vector<uint32>& idx, float r = 0.5f, float h = 1.f, Vec3 col = { 0.8f,0.8f,0.8f }, int slices = 24)
	{
		v.clear(); idx.clear();
		const float PI = 3.14159265358979f, hy = h * 0.5f;
		// 옆면
		for (int j = 0; j <= slices; ++j)
		{
			float th = 2.f * PI * j / slices, cx = cosf(th), sz = sinf(th);
			Vec3 n{ cx, 0, sz }, t{ -sz, 0, cx };
			v.push_back({ {cx * r, hy, sz * r}, n, col, {(float)j / slices, 0}, t });
			v.push_back({ {cx * r,-hy, sz * r}, n, col, {(float)j / slices, 1}, t });
		}
		for (int j = 0; j < slices; ++j)
		{
			uint32 a = j * 2;
			idx.push_back(a); idx.push_back(a + 1); idx.push_back(a + 2);
			idx.push_back(a + 2); idx.push_back(a + 1); idx.push_back(a + 3);
		}
		// 캡 (위 +Y, 아래 -Y)
		for (int s = 0; s < 2; ++s)
		{
			float y = s == 0 ? hy : -hy; Vec3 n{ 0, s == 0 ? 1.f : -1.f, 0 };
			uint32 c = (uint32)v.size(); v.push_back({ {0, y, 0}, n, col, {0.5f,0.5f}, {1,0,0} });
			uint32 base = (uint32)v.size();
			for (int j = 0; j <= slices; ++j) { float th = 2.f * PI * j / slices; v.push_back({ {cosf(th) * r, y, sinf(th) * r}, n, col, {0,0}, {1,0,0} }); }
			for (int j = 0; j < slices; ++j)
			{
				if (s == 0) { idx.push_back(c); idx.push_back(base + j); idx.push_back(base + j + 1); }
				else { idx.push_back(c); idx.push_back(base + j + 1); idx.push_back(base + j); }
			}
		}
	}

	// 원뿔 — 밑면 반지름 r, 높이 h, 꼭대기 +Y
	inline void CreateCone(vector<Vtx>& v, vector<uint32>& idx, float r = 0.5f, float h = 1.f, Vec3 col = { 0.8f,0.8f,0.8f }, int slices = 24)
	{
		v.clear(); idx.clear();
		const float PI = 3.14159265358979f, hy = h * 0.5f;
		Vec3 apex{ 0, hy, 0 };
		for (int j = 0; j < slices; ++j)
		{
			float t0 = 2.f * PI * j / slices, t1 = 2.f * PI * (j + 1) / slices;
			Vec3 p0{ cosf(t0) * r, -hy, sinf(t0) * r }, p1{ cosf(t1) * r, -hy, sinf(t1) * r };
			Vec3 e0{ p1.x - p0.x, p1.y - p0.y, p1.z - p0.z }, e1{ apex.x - p0.x, apex.y - p0.y, apex.z - p0.z };
			Vec3 n{ e0.y * e1.z - e0.z * e1.y, e0.z * e1.x - e0.x * e1.z, e0.x * e1.y - e0.y * e1.x };
			float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z); if (l > 1e-6f) { n.x /= l; n.y /= l; n.z /= l; }
			uint32 b = (uint32)v.size();
			v.push_back({ p0, n, col, {0,1}, {1,0,0} }); v.push_back({ p1, n, col, {1,1}, {1,0,0} }); v.push_back({ apex, n, col, {0.5f,0}, {1,0,0} });
			idx.push_back(b); idx.push_back(b + 2); idx.push_back(b + 1);
		}
		// 밑면
		uint32 c = (uint32)v.size(); v.push_back({ {0,-hy,0}, {0,-1,0}, col, {0.5f,0.5f}, {1,0,0} });
		uint32 base = (uint32)v.size();
		for (int j = 0; j <= slices; ++j) { float th = 2.f * PI * j / slices; v.push_back({ {cosf(th) * r,-hy,sinf(th) * r}, {0,-1,0}, col, {0,0}, {1,0,0} }); }
		for (int j = 0; j < slices; ++j) { idx.push_back(c); idx.push_back(base + j + 1); idx.push_back(base + j); }
	}

	// 토러스 — 큰 반지름 R, 튜브 반지름 r
	inline void CreateTorus(vector<Vtx>& v, vector<uint32>& idx, float R = 0.35f, float r = 0.15f, Vec3 col = { 0.8f,0.8f,0.8f }, int rings = 24, int sides = 16)
	{
		v.clear(); idx.clear();
		const float PI = 3.14159265358979f;
		for (int i = 0; i <= rings; ++i)
		{
			float u = 2.f * PI * i / rings, cu = cosf(u), su = sinf(u);
			for (int j = 0; j <= sides; ++j)
			{
				float vv = 2.f * PI * j / sides, cv = cosf(vv), sv = sinf(vv);
				Vec3 n{ cv * cu, sv, cv * su };
				Vec3 p{ (R + r * cv) * cu, r * sv, (R + r * cv) * su };
				v.push_back({ p, n, col, {(float)i / rings, (float)j / sides}, {-su,0,cu} });
			}
		}
		const int ring = sides + 1;
		for (int i = 0; i < rings; ++i)
			for (int j = 0; j < sides; ++j)
			{
				uint32 a = i * ring + j, b = a + ring;
				idx.push_back(a); idx.push_back(a + 1); idx.push_back(b);
				idx.push_back(b); idx.push_back(a + 1); idx.push_back(b + 1);
			}
	}

	// 캡슐 — 원기둥(높이 h) + 반구 캡 2개 (반지름 r)
	inline void CreateCapsule(vector<Vtx>& v, vector<uint32>& idx, float r = 0.35f, float h = 0.6f, Vec3 col = { 0.8f,0.8f,0.8f }, int slices = 24, int stacks = 8)
	{
		v.clear(); idx.clear();
		const float PI = 3.14159265358979f, hy = h * 0.5f;
		auto ring = [&](float y, float rad, float ny) { uint32 b = (uint32)v.size();
			for (int j = 0; j <= slices; ++j) { float th = 2.f * PI * j / slices, cx = cosf(th), sz = sinf(th);
				Vec3 n{ cx * (1 - fabsf(ny)), ny, sz * (1 - fabsf(ny)) }; float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z); if (l > 1e-6f) { n.x /= l; n.y /= l; n.z /= l; }
				v.push_back({ {cx * rad, y, sz * rad}, n, col, {(float)j / slices, 0}, {-sz,0,cx} }); } return b; };
		std::vector<uint32> rings;
		// 상단 반구
		for (int i = 0; i <= stacks; ++i) { float a = (PI * 0.5f) * i / stacks; rings.push_back(ring(hy + cosf(a) * r, sinf(a) * r, cosf(a))); }
		// 하단 반구
		for (int i = 0; i <= stacks; ++i) { float a = (PI * 0.5f) * i / stacks; rings.push_back(ring(-hy - sinf(a) * r, cosf(a) * r, -sinf(a))); }
		for (size_t k = 0; k + 1 < rings.size(); ++k)
			for (int j = 0; j < slices; ++j)
			{
				uint32 a = rings[k] + j, b = rings[k + 1] + j;
				idx.push_back(a); idx.push_back(a + 1); idx.push_back(b);
				idx.push_back(b); idx.push_back(a + 1); idx.push_back(b + 1);
			}
	}
}
