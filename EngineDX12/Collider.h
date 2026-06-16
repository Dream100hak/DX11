#pragma once
#include "Component.h"
#include "Transform.h"
#include "imgui.h"

// DX11 Engine/BaseCollider·AABBBoxCollider·SphereCollider 이식 (DX12 — GPU 버퍼 없이 DebugDraw 시각화).
// 레이 교차로 정밀 픽킹, GameObject 월드행렬 따라 이동.
enum class ColliderType : uint8 { Sphere, AABB, OBB };

class BaseCollider : public Component
{
public:
	BaseCollider(ColliderType t) : Component(ComponentType::Collider), _colliderType(t) {}
	virtual ~BaseCollider() {}

	// 월드 레이(ro,rd) 교차 → 거리. (rd 정규화 가정)
	virtual bool Intersects(const Vec3& ro, const Vec3& rd, float& dist) = 0;

	ColliderType GetColliderType() const { return _colliderType; }
	Vec3 _center{ 0.f, 0.f, 0.f }; // 로컬 오프셋

protected:
	ColliderType _colliderType;

	// 로컬 _center → 월드 좌표 (GameObject 월드행렬)
	DirectX::XMVECTOR WorldCenter()
	{
		using namespace DirectX;
		XMVECTOR c = XMVectorSet(_center.x, _center.y, _center.z, 1.f);
		if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); c = XMVector3Transform(c, XMLoadFloat4x4(&wm)); }
		return c;
	}
	Vec3 WorldScale()
	{
		if (auto t = GetTransform()) return t->GetScale();
		return Vec3{ 1.f, 1.f, 1.f };
	}
};

// 축 정렬 박스 (월드 스케일 반영, 회전은 무시 = AABB)
class AABBBoxCollider : public BaseCollider
{
public:
	AABBBoxCollider() : BaseCollider(ColliderType::AABB) {}
	Vec3 _extents{ 0.5f, 0.5f, 0.5f }; // 반-크기(로컬)

	void WorldBounds(Vec3& mn, Vec3& mx)
	{
		using namespace DirectX;
		XMFLOAT3 c; XMStoreFloat3(&c, WorldCenter());
		Vec3 s = WorldScale();
		Vec3 e{ _extents.x * fabsf(s.x), _extents.y * fabsf(s.y), _extents.z * fabsf(s.z) };
		mn = Vec3{ c.x - e.x, c.y - e.y, c.z - e.z };
		mx = Vec3{ c.x + e.x, c.y + e.y, c.z + e.z };
	}

	virtual bool Intersects(const Vec3& ro, const Vec3& rd, float& dist) override
	{
		Vec3 mn, mx; WorldBounds(mn, mx);
		const float* ros = &ro.x; const float* rds = &rd.x; const float* bmn = &mn.x; const float* bmx = &mx.x;
		float tmin = 0.f, tmax = 1e9f;
		for (int a = 0; a < 3; ++a)
		{
			if (fabsf(rds[a]) < 1e-7f) { if (ros[a] < bmn[a] || ros[a] > bmx[a]) return false; }
			else { float inv = 1.f / rds[a]; float t1 = (bmn[a] - ros[a]) * inv, t2 = (bmx[a] - ros[a]) * inv;
				if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
				tmin = max(tmin, t1); tmax = min(tmax, t2); if (tmin > tmax) return false; }
		}
		if (tmax < 0) return false;
		dist = max(tmin, 0.f); return true;
	}

	virtual void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Box Collider");
		ImGui::DragFloat3("Center", &_center.x, 0.01f);
		ImGui::DragFloat3("Extents", &_extents.x, 0.01f, 0.01f, 100.f);
	}
};

// 구
class SphereCollider : public BaseCollider
{
public:
	SphereCollider() : BaseCollider(ColliderType::Sphere) {}
	float _radius = 0.5f;

	float WorldRadius() { Vec3 s = WorldScale(); return _radius * max(max(fabsf(s.x), fabsf(s.y)), fabsf(s.z)); }

	virtual bool Intersects(const Vec3& ro, const Vec3& rd, float& dist) override
	{
		using namespace DirectX;
		XMVECTOR c = WorldCenter();
		XMVECTOR o = XMVectorSet(ro.x, ro.y, ro.z, 0.f), d = XMVectorSet(rd.x, rd.y, rd.z, 0.f);
		XMVECTOR oc = XMVectorSubtract(o, c);
		float r = WorldRadius();
		float b = XMVectorGetX(XMVector3Dot(oc, d));
		float cc = XMVectorGetX(XMVector3Dot(oc, oc)) - r * r;
		float disc = b * b - cc;
		if (disc < 0.f) return false;
		float t = -b - sqrtf(disc);
		if (t < 0.f) t = -b + sqrtf(disc);
		if (t < 0.f) return false;
		dist = t; return true;
	}

	virtual void OnInspectorGUI() override
	{
		ImGui::SeparatorText("Sphere Collider");
		ImGui::DragFloat3("Center", &_center.x, 0.01f);
		ImGui::DragFloat("Radius", &_radius, 0.01f, 0.01f, 100.f);
	}
};
