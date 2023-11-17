#pragma once
class Model;

enum class obj3DType
{
	MESH,
	MODEL,
	ANIMATOR,
};


class ShadowRenderer : public Component
{
	using Super = Component;
public:

	ShadowRenderer(uint32 width , uint32 height);
	virtual ~ShadowRenderer();

	void Init();

	shared_ptr<Texture> GetShadowMap() {return _shadowMap;}

	void SetMesh(shared_ptr<Mesh> mesh) { _mesh = mesh; }
	void SetMaterial(shared_ptr<Material> material) { _material = material; }
	void SetPass(uint8 pass) { _pass = pass; }

	void SetModel(shared_ptr< Model> model );

	//void BindDsvAndSetNullRenderTarget();
	void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);

	InstanceID GetInstanceID();
	
private:
	void CreateTexture();

private:
	
	shared_ptr<Texture> _shadowMap; 
	ComPtr<ID3D11ShaderResourceView> _srv;

private:
	shared_ptr<Mesh> _mesh;
	shared_ptr<Model> _model;
	shared_ptr<Material> _material;
	shared_ptr<Shader> _shader;

	uint8 _pass = 0;

	uint32 _width;
	uint32 _height;



};

