#pragma once
#include "Component.h"

class Model;
class ShaderBuffer;
class Material;

class ModelRenderer : public Component
{
	using Super = Component;

public:
	ModelRenderer(shared_ptr<ShaderBuffer> shader);
	virtual ~ModelRenderer();

	void SetModel(shared_ptr<Model> model);
	void SetPass(uint8 pass) { _pass = pass; }

	void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
	InstanceID GetInstanceID();

private:
	shared_ptr<ShaderBuffer>	_shader;
	uint8				_pass = 0;
	shared_ptr<Model>	_model;
};

