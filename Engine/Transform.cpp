#include "pch.h"
#include "Transform.h"
#include "GameObject.h"

// other 의 조상 체인에 자신이 있는지 (순환 부모 방지용)
bool Transform::IsAncestorOf(Transform* other)
{
	for (shared_ptr<Transform> p = other ? other->GetParent() : nullptr; p != nullptr; p = p->GetParent())
	{
		if (p.get() == this)
			return true;
	}
	return false;
}

void Transform::RemoveChild(Transform* child)
{
	_children.erase(
		std::remove_if(_children.begin(), _children.end(),
			[child](const shared_ptr<Transform>& c) { return c.get() == child; }),
		_children.end());
}

// 월드 트랜스폼 유지 재부모화 — 하이라키 드래그앤드롭/삭제 승격의 단일 진입점
void Transform::SetParentKeepWorld(shared_ptr<Transform> newParent)
{
	// 자기 자신/자손에게 부모 지정 금지 (순환 방지)
	if (newParent != nullptr && (newParent.get() == this || IsAncestorOf(newParent.get())))
		return;

	if (GetParent() == newParent)
		return;

	// 부모의 children 에 등록할 자기 자신의 shared_ptr (GameObject 가 소유)
	shared_ptr<Transform> self;
	if (GetGameObject() != nullptr)
		self = GetGameObject()->GetTransform();
	if (self == nullptr)
		return;

	Matrix world = _matWorld;

	if (auto oldParent = GetParent())
		oldParent->RemoveChild(this);

	SetParent(newParent);
	if (newParent != nullptr)
		newParent->AddChild(self);

	SetWorldMatrix(world); // 월드 유지 → 로컬 재계산
	UpdateTransform();
}

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

	if (auto parent = GetParent())
	{
		_matWorld = _matLocal * parent->GetWorldMatrix();
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
	if (auto parent = GetParent())
	{
		Vec3 parentScale = parent->GetScale();
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
	if (auto parent = GetParent())
	{
		Matrix inverseMatrix = parent->GetWorldMatrix().Invert();

		Vec3 rotation;
		rotation.TransformNormal(worldRotation, inverseMatrix);

		SetLocalRotation(rotation);
	}
	else
		SetLocalRotation(worldRotation);
}

void Transform::SetPosition(const Vec3& worldPosition)
{
	if (auto parent = GetParent())
	{
		Matrix worldToParentLocalMatrix = parent->GetWorldMatrix().Invert();

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