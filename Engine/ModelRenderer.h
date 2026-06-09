#pragma once
#include "Component.h"
#include "Renderer.h"
#include "RenderContext.h"

class Model;
class Material;
class InstancingBuffer;

class ModelRenderer : public Renderer
{
	using Super = Renderer;

public:
	ModelRenderer();
	virtual ~ModelRenderer();

	void OnInspectorGUI() override;

	// ���� �ű� ���� ������ ��������������������������������������������������������������������������
	void Draw(const RenderContext& ctx) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

public:
	void SetModel(shared_ptr<Model> model);

	virtual InstanceID GetInstanceID() override;
	shared_ptr<Model>& GetModel() { return _model; }

private:
	shared_ptr<Model>  _model;
};

