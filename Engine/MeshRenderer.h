#pragma once
#include "Material.h"
#include "Renderer.h"
#include "RenderContext.h"

class Mesh;
class Material;
class Shader;
class InstancingBuffer;

#define MAX_MESH_INSTANCE 500

class MeshRenderer : public Renderer
{
	using Super = Renderer;

public:
	MeshRenderer();
	virtual ~MeshRenderer();
	void OnInspectorGUI() override;

	// ── 신규 단일 진입점 ─────────────────────────────────────
	void Draw(const RenderContext& ctx) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

public:
	virtual InstanceID GetInstanceID() override;

	shared_ptr<Material>& GetMaterial() { return _material; }
	shared_ptr<Mesh>&     GetMesh()     { return _mesh; }

	void SetMesh(shared_ptr<Mesh> mesh)   { _mesh     = mesh; }
	void SetMaterial(shared_ptr<Material> material) { _material = material; }

private:
	shared_ptr<Mesh>     _mesh     = nullptr;
	shared_ptr<Material> _material = nullptr;
};