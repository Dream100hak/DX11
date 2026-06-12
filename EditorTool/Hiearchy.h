#pragma once
#include "EditorWindow.h"

class Hiearchy : public EditorWindow
{
public:
	Hiearchy(Vec2 pos, Vec2 size);
	virtual ~Hiearchy();

	virtual void Init() override;
	virtual void Update() override;

	void ShowHiearchy();
	void DrawHierarchyNode(shared_ptr<GameObject> obj); // 트리 노드 재귀 렌더 + 드래그앤드롭 페어런팅


public:
	
	int32 CreateFire();
	int32 CreateRain();
	int32 CreateSky();
	int32 CreateTerrain();
	int32 CreateGameCamera();
	void CreatePbrTestGrid();

};

