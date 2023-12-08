#pragma once
#include "Component.h"

class Mesh;
class Shader;
class Material;

#define MAX_MESH_INSTANCE 500

class MeshRenderer : public Component
{
	using Super = Component;
public:
	MeshRenderer();
	virtual ~MeshRenderer();

	void OnInspectorGUI() override;

	void SetMesh(shared_ptr<Mesh> mesh) { _mesh = mesh; }
	void SetMaterial(shared_ptr<Material> material) { _material = material; }
	void SetPass(uint8 pass) { _pass = pass; }
	void SetTechnique(uint8 teq) { _teq = teq; }

	void PreRenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
	void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);

	void TransformBoundingBox();

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance);

	InstanceID GetInstanceID();
	shared_ptr<Material>& GetMaterial() { return _material;}
	shared_ptr<Mesh>& GetMesh() { return _mesh; }

private:
	shared_ptr<Mesh> _mesh;
	shared_ptr<Material> _material;

	uint8 _pass = 0;
	uint8 _teq = 0;

	BoundingBox _boundingBox;
};