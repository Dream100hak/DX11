#pragma once
#include "ResourceBase.h"

struct ModelBone;
struct ModelMesh;
struct ModelAnimation;

class Model : public ResourceBase
{
	using Super = ResourceBase;

public:
	Model();
	virtual ~Model();

public:
	void ReadMaterial(wstring filename);
	void ReadMaterialByXml(wstring filename);
	void ReadModel(wstring filename);
	void ReadAnimation(wstring filename);

	uint32 GetMaterialCount() { return static_cast<uint32>(_materials.size()); }
	vector<shared_ptr<Material>>& GetMaterials() { return _materials; }
	shared_ptr<Material> GetMaterialByIndex(uint32 index) { return _materials[index]; }
	shared_ptr<Material> GetMaterialByName(const wstring& name);

	uint32 GetMeshCount() { return static_cast<uint32>(_meshes.size()); }
	vector<shared_ptr<ModelMesh>>& GetMeshes() { return _meshes; }
	shared_ptr<ModelMesh> GetMeshByIndex(uint32 index) { return _meshes[index]; }
	shared_ptr<ModelMesh> GetMeshByName(const wstring& name);

	uint32 GetBoneCount() { return static_cast<uint32>(_bones.size()); }
	vector<shared_ptr<ModelBone>>& GetBones() { return _bones; }
	shared_ptr<ModelBone> GetBoneByIndex(uint32 index) { return (index < 0 || index >= _bones.size() ? nullptr : _bones[index]); }
	shared_ptr<ModelBone> GetBoneByName(const wstring& name);

	uint32 GetAnimationCount() { return _animations.size(); }
	vector<shared_ptr<ModelAnimation>>& GetAnimations() { return _animations; }
	shared_ptr<ModelAnimation> GetAnimationByIndex(UINT index) { return (index < 0 || index >= _animations.size()) ? nullptr : _animations[index]; }
	shared_ptr<ModelAnimation> GetAnimationByFileName(wstring name);
	shared_ptr<ModelAnimation> GetAnimationByClipName(wstring name);
	
	int32 GetAnimIndexByFileName(wstring name);
	int32 GetAnimIndexByClipName(wstring name);

	BoundingBox CalculateModelBoundingBox();

private:
	void BindCacheInfo();


private:
	wstring _modelPath = L"../Resources/Assets/Models/";
	wstring _texturePath = L"../Resources/Assets/Textures/";

private:
	shared_ptr<ModelBone> _root;
	vector<shared_ptr<Material>> _materials;
	vector<shared_ptr<ModelBone>> _bones;
	vector<shared_ptr<ModelMesh>> _meshes;
	vector<shared_ptr<ModelAnimation>> _animations;

};

