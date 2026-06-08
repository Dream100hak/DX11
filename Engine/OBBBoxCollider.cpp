#include "pch.h"
#include "OBBBoxCollider.h"
#include "SphereCollider.h"
#include "AABBBoxCollider.h"
#include "ResourceManager.h"
#include "GeometryHelper.h"
#include "Material.h"
#include "Camera.h"
#include "HlslShader.h"

OBBBoxCollider::OBBBoxCollider() : BaseCollider(ColliderType::OBB)
{

}

OBBBoxCollider::~OBBBoxCollider()
{

}

void OBBBoxCollider::Start()
{
	auto material = RESOURCES->Get<Material>(L"Collider");
	if (material == nullptr)
	{
		material = make_shared<Material>();
		material->SetHlslShader(RESOURCES->Get<HlslShader>(L"Collider_HLSL"));
		MaterialDesc& desc = material->GetMaterialDesc();
		desc.diffuse = Vec4(0.f, 1.f, 0.f, 1.f);

		RESOURCES->Add(L"Collider", material);
	}

	_material = material;

	_geometry = make_shared<Geometry<VertexColorData>>();
	GeometryHelper::CreateOBB(_geometry , material->GetMaterialDesc().diffuse , _boundingBox );
	CreateBuffers();
}


void OBBBoxCollider::Update()
{
	if (_material == nullptr || _geometry == nullptr)
		return;

	auto shader = _material->GetHlslShader();
	if (shader == nullptr)
		return;

	_boundingBox.Center = GetTransform()->GetLocalPosition() + _offset;

	Matrix world;
	Matrix matScale = Matrix::CreateScale(_boundingBox.Extents);
	Matrix matTranslation = Matrix::CreateTranslation(_boundingBox.Center);

	world = matScale * matTranslation;

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());
	shader->PushTransformData(TransformDesc{ world });

	GetVertexBuffer()->PushData();
	GetIndexBuffer()->PushData();

	shader->DrawLineIndexed(GetIndexBuffer()->GetCount());
}

bool OBBBoxCollider::Intersects(Ray& ray, OUT float& distance)
{
	return _boundingBox.Intersects(ray.position, ray.direction, OUT distance);
}

bool OBBBoxCollider::Intersects(shared_ptr<BaseCollider>& other)
{
	ColliderType type = other->GetColliderType();

	switch (type)
	{
	case ColliderType::Sphere:
		return _boundingBox.Intersects(dynamic_pointer_cast<SphereCollider>(other)->GetBoundingSphere());
	case ColliderType::AABB:
		return _boundingBox.Intersects(dynamic_pointer_cast<AABBBoxCollider>(other)->GetBoundingBox());
	case ColliderType::OBB:
		return _boundingBox.Intersects(dynamic_pointer_cast<OBBBoxCollider>(other)->GetBoundingBox());
	}

	return false;
}

