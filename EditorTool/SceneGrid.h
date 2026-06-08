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

	void Init(int32 count , float size);
	virtual void Update() override;

	void Draw(const RenderContext& ctx) override;

	void DrawGrid(Matrix V, Matrix P);

private:
	
	shared_ptr<Geometry<VertexTextureData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	shared_ptr<class HlslShader> _shader;

};

