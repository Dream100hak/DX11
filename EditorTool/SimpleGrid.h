#pragma once
#include "MonoBehaviour.h"

class SimpleGrid : public MonoBehaviour
{

public:
	void Create(int32 sizeX, int32 sizeZ, shared_ptr<Material> material);
	virtual void Update() override;

	int32 GetSizeX() { return _sizeX; }
	int32 GetSizeZ() { return _sizeZ; }

	shared_ptr<Mesh> GetMesh() { return _mesh; }

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance);

private:
	shared_ptr<Mesh> _mesh;
	int32 _sizeX = 0;
	int32 _sizeZ = 0;
};

