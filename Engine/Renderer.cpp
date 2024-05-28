#include "pch.h"
#include "Renderer.h"
#include "MathUtils.h"
#include "Utils.h"
#include "Material.h"

Renderer::Renderer(RendererType type) : Super(ComponentType::Renderer) , _renderType(type)
{
	auto mat = RESOURCES->Get<Material>(L"DefaultMaterial");

	_shadowMap = mat->GetShadowMap();
	_ssaoMap = mat->GetSsaoMap();
}

Renderer::~Renderer()
{
}


void Renderer::TransformBoundingBox()
{
	Matrix W = GetTransform()->GetWorldMatrix();

	Vec3 vMin = Vec3(MathUtils::INF, MathUtils::INF, MathUtils::INF);
	Vec3 vMax = Vec3(-MathUtils::INF, -MathUtils::INF, -MathUtils::INF);

	Vec3 corners[8];
	BoundingBox modelBox;
	modelBox.Center = Vec3(0.f, 1.0f, 0.f);
	modelBox.Extents = Vec3(1.0f, 1.0f, 1.0f);
	modelBox.GetCorners(corners);

	for (int i = 0; i < 8; ++i) {
		corners[i] = XMVector3TransformCoord(corners[i], W);
	}
	for (const Vec3& corner : corners) {
		vMin = ::XMVectorMin(vMin, corner);
		vMax = ::XMVectorMax(vMax, corner);
	}
	_boundingBox.Center = 0.5f * (vMin + vMax);
	_boundingBox.Extents = 0.5f * (vMax - vMin);
}

InstanceID Renderer::GetInstanceID()
{
	return make_pair(0,0);
}

