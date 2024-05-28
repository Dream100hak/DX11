#pragma once
#include "Frustum.h"
#include "Renderer.h"
#include "MonoBehaviour.h"



class SceneGrid : public Renderer
{
	using Super = Renderer;

public: 
	SceneGrid();
	virtual ~SceneGrid();

	void Init();
	virtual void Update() override;

	void Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light) override;
	void RenderInstancing(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer) override;
	void RenderThumbnail(int32 tech, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer) override;

	void DrawGrid(Matrix V, Matrix P);

private:
	
	shared_ptr<Geometry<VertexTextureData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	shared_ptr<Shader> _shader; 
	uint8 _pass = 0;

};

