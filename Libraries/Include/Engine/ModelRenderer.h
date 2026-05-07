#pragma once
#include "Component.h"
#include "Renderer.h"
#include "RenderContext.h"

class Model;
class Shader;
class Material;
class InstancingBuffer;

class ModelRenderer : public Renderer
{
	using Super = Renderer;

public:
	ModelRenderer(shared_ptr<Shader> shader);
	virtual ~ModelRenderer();

	void OnInspectorGUI() override;

	// ── 신규 단일 진입점 ─────────────────────────────────────
	void Draw(const RenderContext& ctx) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

public:
	void ChangeShader(shared_ptr<Shader> shader);
	void SetModel(shared_ptr<Model> model);

	virtual InstanceID GetInstanceID() override;
	shared_ptr<Model>& GetModel() { return _model; }

private:
	void PushMeshes(const RenderContext& ctx, bool instanced);

	shared_ptr<Model>  _model;
	shared_ptr<Shader> _shader;
};

