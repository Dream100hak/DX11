#pragma once
#include "Component.h"
#include "Renderer.h "

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

	void Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light) override;
	void RenderInstancing(int32 tech , shared_ptr<Shader> shader , Matrix V, Matrix P, shared_ptr<Light> light,  shared_ptr<InstancingBuffer>& buffer) override;
	void RenderThumbnail(int32 tech, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

	void PushBuffer(uint8 technique, uint8 pass , shared_ptr<Light> light);
	void PushBufferInstancing(uint8 technique, uint8 pass, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer);

public:
	void ChangeShader(shared_ptr<Shader> shader);
	void SetModel(shared_ptr<Model> model);

	virtual InstanceID GetInstanceID() override;
	shared_ptr<Model>& GetModel() {return _model;}

private:

	shared_ptr<Model>  _model;
	shared_ptr<Shader> _shader;

};

