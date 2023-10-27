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

		float uiPos[3] = { _localPosition.x , _localPosition.y , _localPosition.z };
		float uiRot[3] = { _localRotation.x , _localRotation.y ,_localRotation.z };
		float uiScale[3] = { _localScale.x , _localScale.y ,_localScale.z };

		ImGui::Text("Pos		");
		ImGui::SameLine(0.f, -2.f);

		if (ImGui::DragFloat3("##pos", uiPos))
		{
			SetLocalPosition(Vec3(uiPos));

		}
		ImGui::Text("Rot		");
		ImGui::SameLine();

		if (ImGui::DragFloat3("##rot", uiRot , 1.f , -360.f , 360.f))
		{
			SetLocalRotation(Vec3(uiRot));
		}

		ImGui::Text("Scale	  ");
		ImGui::SameLine();

		if (ImGui::DragFloat3("##scale", uiScale))
		{
			SetLocalScale(Vec3(uiScale));
		}

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

	// World
	Vec3 GetScale() { return _scale; }
	void SetScale(const Vec3& scale);
	Vec3 GetRotation() { return _rotation; }
	void SetRotation(const Vec3& rotation);
	Vec3 GetPosition() { return _position; }
	void SetPosition(const Vec3& position);

	Vec3 GetRight() { return _matWorld.Right(); }
	Vec3 GetUp() { return _matWorld.Up(); }
	Vec3 GetLook() { return _matWorld.Backward(); }

	Matrix GetWorldMatrix() { return _matWorld; }

	// °èÃþ °ü°è
	bool HasParent() { return _parent != nullptr; }

	shared_ptr<Transform> GetParent() { return _parent; }
	void SetParent(shared_ptr<Transform> parent) { _parent = parent; }

	const vector<shared_ptr<Transform>>& GetChildren() { return _children; }
	void AddChild(shared_ptr<Transform> child) { _children.push_back(child); }

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
	shared_ptr<Transform> _parent;
	vector<shared_ptr<Transform>> _children;
};

