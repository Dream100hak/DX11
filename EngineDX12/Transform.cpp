#include "Transform.h"
#include "imgui.h"

using namespace DirectX;

Transform::Transform() : Component(ComponentType::Transform)
{
	XMStoreFloat4x4(&_matLocal, XMMatrixIdentity());
	XMStoreFloat4x4(&_matWorld, XMMatrixIdentity());
}

void Transform::Awake() { UpdateTransform(); }
void Transform::Update() {}

void Transform::UpdateTransform()
{
	// 로컬 = S * R(오일러) * T
	XMMATRIX S = XMMatrixScaling(_localScale.x, _localScale.y, _localScale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(_localRotation.x, _localRotation.y, _localRotation.z);
	XMMATRIX T = XMMatrixTranslation(_localPosition.x, _localPosition.y, _localPosition.z);
	XMMATRIX local = S * R * T;
	XMStoreFloat4x4(&_matLocal, local);

	XMMATRIX world = local;
	if (auto parent = _parent.lock())
		world = local * XMLoadFloat4x4(&parent->_matWorld);
	XMStoreFloat4x4(&_matWorld, world);

	// 월드 분해 (위치/회전/스케일 캐시)
	XMVECTOR s, q, t;
	if (XMMatrixDecompose(&s, &q, &t, world))
	{
		XMStoreFloat3(&_scale, s);
		XMStoreFloat3(&_position, t);
		// 쿼터니언 → 오일러(라디안) 근사
		XMFLOAT4 qf; XMStoreFloat4(&qf, q);
		float sinp = 2.f * (qf.w * qf.x - qf.y * qf.z);
		_rotation.x = (fabsf(sinp) >= 1.f) ? copysignf(XM_PIDIV2, sinp) : asinf(sinp);
		_rotation.y = atan2f(2.f * (qf.w * qf.y + qf.z * qf.x), 1.f - 2.f * (qf.x * qf.x + qf.y * qf.y));
		_rotation.z = atan2f(2.f * (qf.w * qf.z + qf.x * qf.y), 1.f - 2.f * (qf.z * qf.z + qf.x * qf.x));
	}

	++_version; // 더티 (렌더러 재베이크 트리거)

	// 자식 전파
	for (auto& c : _children)
		if (c) c->UpdateTransform();
}

void Transform::SetWorldMatrix(const XMFLOAT4X4& matWorld)
{
	++_version;
	_matWorld = matWorld;
	XMMATRIX world = XMLoadFloat4x4(&matWorld);

	XMMATRIX local = world;
	if (auto parent = _parent.lock())
		local = world * XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent->_matWorld));
	XMStoreFloat4x4(&_matLocal, local);

	XMVECTOR s, q, t;
	if (XMMatrixDecompose(&s, &q, &t, local))
	{
		XMStoreFloat3(&_localScale, s);
		XMStoreFloat3(&_localPosition, t);
		XMFLOAT4 qf; XMStoreFloat4(&qf, q);
		float sinp = 2.f * (qf.w * qf.x - qf.y * qf.z);
		_localRotation.x = (fabsf(sinp) >= 1.f) ? copysignf(XM_PIDIV2, sinp) : asinf(sinp);
		_localRotation.y = atan2f(2.f * (qf.w * qf.y + qf.z * qf.x), 1.f - 2.f * (qf.x * qf.x + qf.y * qf.y));
		_localRotation.z = atan2f(2.f * (qf.w * qf.z + qf.x * qf.y), 1.f - 2.f * (qf.z * qf.z + qf.x * qf.x));
	}
}

void Transform::SetPosition(const XMFLOAT3& worldPos)
{
	// 부모가 있으면 월드→로컬 역변환 후 로컬 위치 갱신
	if (auto parent = _parent.lock())
	{
		XMVECTOR wp = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.f);
		XMVECTOR lp = XMVector3Transform(wp, XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent->_matWorld)));
		XMStoreFloat3(&_localPosition, lp);
	}
	else _localPosition = worldPos;
	UpdateTransform();
}

static XMFLOAT3 RowDir(const XMFLOAT4X4& m, int row)
{
	const float* r = &m.m[row][0];
	XMVECTOR v = XMVector3Normalize(XMVectorSet(r[0], r[1], r[2], 0.f));
	XMFLOAT3 o; XMStoreFloat3(&o, v); return o;
}
XMFLOAT3 Transform::GetRight() { return RowDir(_matWorld, 0); }
XMFLOAT3 Transform::GetUp()    { return RowDir(_matWorld, 1); }
XMFLOAT3 Transform::GetLook()  { return RowDir(_matWorld, 2); }

void Transform::RemoveChild(Transform* child)
{
	for (auto it = _children.begin(); it != _children.end(); ++it)
		if (it->get() == child) { _children.erase(it); return; }
}

bool Transform::IsAncestorOf(Transform* other)
{
	for (Transform* p = other; p; )
	{
		auto par = p->_parent.lock();
		if (!par) break;
		if (par.get() == this) return true;
		p = par.get();
	}
	return false;
}

void Transform::SetParentKeepWorld(shared_ptr<Transform> newParent)
{
	// 자기 자신/자손을 부모로 지정 거부 (사이클 가드)
	if (newParent.get() == this) return;
	if (newParent && IsAncestorOf(newParent.get())) return;

	XMFLOAT4X4 keepWorld = _matWorld;

	if (auto old = _parent.lock())
		old->RemoveChild(this);

	_parent = newParent;
	if (newParent)
		newParent->_children.push_back(GetTransform()); // 소유 GameObject 의 Transform shared_ptr

	SetWorldMatrix(keepWorld); // 월드 유지하며 로컬 재계산
	UpdateTransform();
}

void Transform::SetParentKeepLocal(shared_ptr<Transform> newParent)
{
	// 로컬 TRS 유지(이미 설정됨) — 월드만 재계산. 씬 로드 시 LOCAL-preserving 링크.
	if (newParent.get() == this) return;
	if (newParent && IsAncestorOf(newParent.get())) return;
	if (auto old = _parent.lock()) old->RemoveChild(this);
	_parent = newParent;
	if (newParent) newParent->_children.push_back(GetTransform());
	UpdateTransform();
}

void Transform::OnInspectorGUI()
{
	const ImVec4 col(0.85f, 0.94f, 0.f, 1.f);
	ImGui::TextColored(col, "Position"); ImGui::SameLine();
	ImGui::DragFloat3("##pos", (float*)&_localPosition, 0.01f);
	ImGui::TextColored(col, "Rotation"); ImGui::SameLine();
	ImGui::DragFloat3("##rot", (float*)&_localRotation, 0.01f);
	ImGui::TextColored(col, "Scale   "); ImGui::SameLine();
	ImGui::DragFloat3("##scale", (float*)&_localScale, 0.01f);
	UpdateTransform();
}
