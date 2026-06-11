#include "pch.h"
#include "Transform.h"

Transform::Transform() : Super(ComponentType::Transform)
{

}

Transform::~Transform()
{

}

void Transform::Awake()
{
}

void Transform::Update()
{
}

Vec3 Transform::ToEulerAngles(Quaternion q)
{
	Vec3 angles;

	// roll (x-axis rotation)
	double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	angles.x = std::atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	// 부동소수 오차로 |t| > 1 이면 sqrt(음수) = NaN → 트랜스폼 전체 오염되므로 반드시 클램프
	double t = 2 * (q.w * q.y - q.x * q.z);
	if (t > 1.0) t = 1.0;
	else if (t < -1.0) t = -1.0;
	double sinp = std::sqrt(1 + t);
	double cosp = std::sqrt(1 - t);
	angles.y = 2 * std::atan2(sinp, cosp) - 3.14159265358979 / 2;

	// yaw (z-axis rotation)
	double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	angles.z = std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

void Transform::UpdateTransform()
{
	Matrix matScale = Matrix::CreateScale(_localScale);
	Matrix matRotation = Matrix::CreateRotationX(_localRotation.x);
	matRotation *= Matrix::CreateRotationY(_localRotation.y);
	matRotation *= Matrix::CreateRotationZ(_localRotation.z);
	Matrix matTranslation = Matrix::CreateTranslation(_localPosition);

	_matLocal = matScale * matRotation * matTranslation;

	if (HasParent())
	{
		_matWorld = _matLocal * _parent->GetWorldMatrix();
	}
	else
	{
		_matWorld = _matLocal;
	}

	Quaternion quat;
	_matWorld.Decompose(_scale, quat, _position);
	_rotation = ToEulerAngles(quat);

	// Children
	for (const shared_ptr<Transform>& child : _children)
		child->UpdateTransform();
}

void Transform::SetScale(const Vec3& worldScale)
{
	if (HasParent())
	{
		Vec3 parentScale = _parent->GetScale();
		Vec3 scale = worldScale;
		scale.x /= parentScale.x;
		scale.y /= parentScale.y;
		scale.z /= parentScale.z;
		SetLocalScale(scale);
	}
	else
	{
		SetLocalScale(worldScale);
	}
}

void Transform::SetRotation(const Vec3& worldRotation)
{
	if (HasParent())
	{
		Matrix inverseMatrix = _parent->GetWorldMatrix().Invert();

		Vec3 rotation;
		rotation.TransformNormal(worldRotation, inverseMatrix);

		SetLocalRotation(rotation);
	}
	else
		SetLocalRotation(worldRotation);
}

void Transform::SetPosition(const Vec3& worldPosition)
{
	if (HasParent())
	{
		Matrix worldToParentLocalMatrix = _parent->GetWorldMatrix().Invert();

		Vec3 position;
		position.Transform(worldPosition, worldToParentLocalMatrix);

		SetLocalPosition(position);
	}
	else
	{
		SetLocalPosition(worldPosition);
	}
}

void Transform::LookAt(const Vec3& targetPosition)
{
	Vec3 eyePosition = GetLocalPosition();
	Vec3 upDirection = GetUp(); 

	Vec3 lookDirection = targetPosition - eyePosition;
	lookDirection.Normalize();

	Vec3 rightDirection;
	rightDirection.Cross(upDirection, lookDirection);
	rightDirection.Normalize();

	upDirection.Cross(lookDirection, rightDirection);

	Matrix rotationMatrix = Matrix::CreateLookAt(eyePosition, targetPosition, upDirection);

	Quaternion lookRotation =  Quaternion::CreateFromRotationMatrix(rotationMatrix);
	Vec3 eulerAngles = Transform::ToEulerAngles(lookRotation);
	SetLocalRotation(eulerAngles);
}

void Transform::Pitch(float angle)
{
	Vec3 rot = _localRotation;
	rot.x += angle;
	SetLocalRotation(rot);
}

void Transform::Yaw(float angle)
{

	Vec3 rot = _localRotation;
	rot.y += angle;
	SetLocalRotation(rot);
}

void Transform::Roll(float angle)
{
	Vec3 rot = _localRotation;
	rot.z += angle;
	SetLocalRotation(rot);
}