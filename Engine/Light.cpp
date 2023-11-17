#include "pch.h"
#include "Light.h"

Matrix Light::S_MatView = Matrix::Identity;
Matrix Light::S_MatProjection = Matrix::Identity;
Matrix Light::S_Shadow = Matrix::Identity;

Light::Light() : Component(ComponentType::Light)
{
	_sceneBounds.Center = Vec3::Zero;
	_sceneBounds.Radius = sqrtf(20000.f);
}

Light::~Light()
{

}

void Light::Update()
{
	if (_type == Directional)
	{
		UpdateMatrix();
	}
}

void Light::UpdateMatrix()
{
	// Only the first "main" light casts a shadow.
	Vec3 lightDir = _desc.direction;
	Vec3 lightPos = -2.0f * _sceneBounds.Radius * lightDir;
	Vec3 upDirection = Vec3::Up;
	Vec3 targetPos = _sceneBounds.Center;
	Vec3 up = Vec3::Up;

	XMMATRIX V = ::XMMatrixLookAtLH(lightPos, targetPos, upDirection);
	
	S_MatView = V;
	// Transform bounding sphere to light space.

	Vec3 sphereCenterLS = Vec3::Transform(targetPos, V);

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - _sceneBounds.Radius;
	float b = sphereCenterLS.y - _sceneBounds.Radius;
	float n = sphereCenterLS.z - _sceneBounds.Radius;
	float r = sphereCenterLS.x + _sceneBounds.Radius;
	float t = sphereCenterLS.y + _sceneBounds.Radius;
	float f = sphereCenterLS.z + _sceneBounds.Radius;
	XMMATRIX P = ::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
	S_MatProjection = P;

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	S_Shadow = V * P * T;


}
