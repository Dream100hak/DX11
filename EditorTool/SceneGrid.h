#pragma once
#include "Frustum.h"
#include "MonoBehaviour.h"

class SceneGrid : public MonoBehaviour
{
public: 
	SceneGrid();
	SceneGrid(shared_ptr<Camera> cam);

	virtual void Start() override;
	virtual void Update() override;

	void DrawGrid();

private:
	
	shared_ptr<Geometry<VertexColorData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;
	shared_ptr<Frustum> _frustum ; 

	shared_ptr<Camera> _cam;

	shared_ptr<Shader> _shader; 
	uint8 _pass = 0;

};

