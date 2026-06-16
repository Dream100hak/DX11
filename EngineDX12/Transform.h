#pragma once
#include "Component.h"

// DX11 Engine/Transform.h 이식 — DirectXMath 기반(SimpleMath 대체).
// 로컬 위치/회전(오일러 라디안)/스케일 + 월드 행렬 캐시 + 부모(weak)/자식(shared) 계층.
class Transform : public Component
{
	using Super = Component;
public:
	Transform();
	~Transform() {}

	virtual void Awake() override;
	virtual void Update() override;
	virtual void OnInspectorGUI() override;

	void UpdateTransform(); // 로컬 → 월드 행렬 재계산 (자식까지 전파)

	// ── Local ──
	DirectX::XMFLOAT3 GetLocalScale() { return _localScale; }
	void SetLocalScale(const DirectX::XMFLOAT3& s) { _localScale = s; UpdateTransform(); }
	DirectX::XMFLOAT3 GetLocalRotation() { return _localRotation; } // 라디안
	void SetLocalRotation(const DirectX::XMFLOAT3& r) { _localRotation = r; UpdateTransform(); }
	DirectX::XMFLOAT3 GetLocalPosition() { return _localPosition; }
	void SetLocalPosition(const DirectX::XMFLOAT3& p) { _localPosition = p; UpdateTransform(); }

	void SetWorldMatrix(const DirectX::XMFLOAT4X4& matWorld); // 월드 직접 설정 → 로컬 역산 + 분해

	// ── World ──
	DirectX::XMFLOAT3 GetScale() { return _scale; }
	DirectX::XMFLOAT3 GetRotation() { return _rotation; }
	DirectX::XMFLOAT3 GetPosition() { return _position; }
	void SetPosition(const DirectX::XMFLOAT3& worldPos);

	DirectX::XMFLOAT3 GetRight();    // 월드 기저 (행렬 행)
	DirectX::XMFLOAT3 GetUp();
	DirectX::XMFLOAT3 GetLook();

	DirectX::XMFLOAT4X4 GetWorldMatrix() { return _matWorld; }
	DirectX::XMFLOAT4X4 GetLocalMatrix() { return _matLocal; }

	// ── 계층 ── 부모=weak(순환참조 누수 방지), 자식=shared
	bool HasParent() { return _parent.lock() != nullptr; }
	shared_ptr<Transform> GetParent() { return _parent.lock(); }
	void SetParent(shared_ptr<Transform> parent) { _parent = parent; }
	const vector<shared_ptr<Transform>>& GetChildren() { return _children; }
	void AddChild(shared_ptr<Transform> child) { _children.push_back(child); }
	void RemoveChild(Transform* child);

	void SetParentKeepWorld(shared_ptr<Transform> newParent); // 월드 유지 재부모 (자기/자손 거부)
	void SetParentKeepLocal(shared_ptr<Transform> newParent); // 로컬 유지 재부모 (씬 로드용)
	bool IsAncestorOf(Transform* other);

	uint32 Version() const { return _version; } // 변경 카운터 (렌더러 더티 체크 — 변경 시에만 재처리)

private:
	DirectX::XMFLOAT3 _localScale = { 1.f, 1.f, 1.f };
	DirectX::XMFLOAT3 _localRotation = { 0.f, 0.f, 0.f };
	DirectX::XMFLOAT3 _localPosition = { 0.f, 0.f, 0.f };

	DirectX::XMFLOAT4X4 _matLocal;
	DirectX::XMFLOAT4X4 _matWorld;

	DirectX::XMFLOAT3 _scale{};
	DirectX::XMFLOAT3 _rotation{};
	DirectX::XMFLOAT3 _position{};

	weak_ptr<Transform>           _parent;
	vector<shared_ptr<Transform>> _children;
	uint32                        _version = 0; // UpdateTransform/SetWorldMatrix 마다 증가
};
