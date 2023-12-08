#pragma once
#include "Component.h"

class Model;
class Shader;
class Material;
class BVH;
struct BVHNode;

class ModelRenderer : public Component
{
	using Super = Component;

public:
	ModelRenderer(shared_ptr<Shader> shader);
	virtual ~ModelRenderer();

	

	void OnInspectorGUI() override;

	void SetModel(shared_ptr<Model> model);
	void SetPass(uint8 pass) { _pass = pass; }

	void ChangeShader(shared_ptr<Shader> shader);

	void PreRenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
	void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
	void PushData(uint8 technique, shared_ptr<class InstancingBuffer>& buffer);
	
	void TransformBoundingBox();

	//void TestRenderStart();
	//void TestRenderBox();

	InstanceID GetInstanceID();

	shared_ptr<Model>& GetModel() {return _model;}

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance);

private:
	shared_ptr<Shader>	_shader;
	uint8				_pass = 0;
	shared_ptr<Model>	_model;


	shared_ptr<Geometry<VertexColorData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	shared_ptr<class Material> _material;
	BoundingBox _boundingBox;

	bool _once = false;

};

