#pragma once
#include "Component.h"
#include "imgui.h"

class Transform : public Component
{
	using Super = Component;
public:
	Transform();
	~Transform();

	virtual void Awake() override;
	virtual void Update() override;

	virtual void OnInspectorGUI() override
	{
		Super::OnInspectorGUI();

		ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

		ImGui::TextColored(color ,"Position  ");
		ImGui::SameLine(0.f, -2.f);

		ImGui::DragFloat3("##pos", (float*)&_localPosition, 0.01f);
		ImGui::TextColored(color ,"Rotation  ");
		ImGui::SameLine();

		ImGui::DragFloat3("##rot", (float*)&_localRotation , 0.01f , -360.f , 360.f);
		ImGui::TextColored(color, "Scale	 ");
		ImGui::SameLine();

		ImGui::DragFloat3("##scale", (float*)&_localScale, 0.01f);
		UpdateTransform();
	}

	void UpdateTransform();

	static Vec3 ToEulerAngles(Quaternion q);

	// Local
	Vec3 GetLocalScale() { return _localScale; }
	void SetLocalScale(const Vec3& localScale) { _localScale = localScale; UpdateTransform(); }
	Vec3 GetLocalRotation() { return _localRotation; }
	void SetLocalRotation(const Vec3& localRotation) { _localRotation = localRotation; UpdateTransform(); }
	Vec3 GetLocalPosition() { return _localPosition; }
	void SetLocalPosition(const Vec3& localPosition) { _localPosition = localPosition; UpdateTransform(); }

	void SetWorldMatrix(const Matrix& matWorld)
	{
		// 월드 행렬 설정
		_matWorld = matWorld;

		// 부모 트랜스폼이 있는 경우
		if (auto parent = GetParent())
		{
			Matrix worldToLocal = parent->GetWorldMatrix().Invert();
			_matLocal = matWorld * worldToLocal;
		}
		else
		{
			_matLocal = matWorld;
		}

		// Decompose the matrix to update local scale, rotation, and position
		Quaternion quat;
		_matLocal.Decompose(_localScale, quat, _localPosition);
		_localRotation = ToEulerAngles(quat);

		//UpdateTransform();
	}

	// World
	Vec3 GetScale() { return _scale; }
	void SetScale(const Vec3& scale);
	Vec3 GetRotation() { return _rotation; }
	void SetRotation(const Vec3& rotation);
	Vec3 GetPosition() { return _position; }
	void SetPosition(const Vec3& position);

	void LookAt(const Vec3& targetPosition);

	Vec3 GetRight() { return _matWorld.Right(); }
	Vec3 GetUp() { return _matWorld.Up(); }
	Vec3 GetLook() { return _matWorld.Backward(); }

	Matrix GetWorldMatrix() { return _matWorld; }
	Matrix GetLocalMatrix() { return _matLocal; }

	// 계층 관계 — 부모는 weak (부모↔자식 shared 양방향이면 순환 참조 누수)
	bool HasParent() { return _parent.lock() != nullptr; }

	shared_ptr<Transform> GetParent() { return _parent.lock(); }
	void SetParent(shared_ptr<Transform> parent) { _parent = parent; }

	const vector<shared_ptr<Transform>>& GetChildren() { return _children; }
	void AddChild(shared_ptr<Transform> child) { _children.push_back(child); }
	void RemoveChild(Transform* child);

	// 월드 트랜스폼을 유지한 채 부모 변경 (nullptr = 루트로). 자기 자신/자손 지정은 거부
	void SetParentKeepWorld(shared_ptr<Transform> newParent);
	bool IsAncestorOf(Transform* other);

	void Pitch(float angle);
	void Yaw(float angle);
	void Roll(float angle);

private:
	Vec3 _localScale = { 1.f, 1.f, 1.f };
	Vec3 _localRotation = { 0.f, 0.f, 0.f };
	Vec3 _localPosition = { 0.f, 0.f, 0.f };

	// Cache
	Matrix _matLocal = Matrix::Identity;
	Matrix _matWorld = Matrix::Identity;

	Vec3 _scale;
	Vec3 _rotation;
	Vec3 _position;

private:
	weak_ptr<Transform> _parent;
	vector<shared_ptr<Transform>> _children;
};

