#pragma once
#include "Component.h"

class Model;
class Shader;
class Material;

class ModelRenderer : public Component
{
	using Super = Component;

public:
	ModelRenderer(shared_ptr<Shader> shader);
	virtual ~ModelRenderer();

	void SetModel(shared_ptr<Model> model);
	void SetPass(uint8 pass) { _pass = pass; }

	void ChangeShader(shared_ptr<Shader> shader);

	void PreRenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
	void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);

	void PushData(shared_ptr<class InstancingBuffer>& buffer);

	InstanceID GetInstanceID();

private:
	shared_ptr<Shader>	_shader;
	uint8				_pass = 0;
	shared_ptr<Model>	_model;
};

