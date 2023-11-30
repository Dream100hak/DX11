#pragma once
#include "BaseCollider.h"

class OBBBoxCollider : public BaseCollider
{
	using Super = Component;
	
public:
	OBBBoxCollider();
	virtual ~OBBBoxCollider();

	virtual void Start() override;
	virtual void Update() override;

	virtual void OnInspectorGUI() override
	{
		Super::OnInspectorGUI();

		float center[3] = { _offset.x,_offset.y, _offset.z };
		float extents[3] = { _boundingBox.Extents.x, _boundingBox.Extents.y, _boundingBox.Extents.z };
		
		ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

		ImGui::TextColored(color ,  "Offset      ");
		ImGui::SameLine(0.f, -2.f);

		if (ImGui::DragFloat3("##center", center))
		{
			_offset = Vec3(center);
		}
		ImGui::TextColored(color, "Extents     ");
		ImGui::SameLine();

		if (ImGui::DragFloat3("##extents", extents))
		{
			_boundingBox.Extents = Vec3(extents);
		}

	}

	virtual bool Intersects(Ray& ray, OUT float& distance) override;
	virtual bool Intersects(shared_ptr<BaseCollider>& other) override;

	BoundingOrientedBox& GetBoundingBox() { return _boundingBox; }


private:

	BoundingOrientedBox _boundingBox;
	shared_ptr<class Material> _material; 
	uint8 _pass = 0;
};

