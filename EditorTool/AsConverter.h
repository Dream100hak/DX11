#pragma once
#include "AsType.h"

class AsConverter
{
public:
	AsConverter();
	~AsConverter();

public:
	void ReadAssetFile(wstring file);
	void ExportModelData(wstring savePath);
	void ExportMaterialData(wstring savePath);
	void ExportAnimationData(wstring savePath, uint32 index = 0);

private:
	void ReadModelData(aiNode* node, int32 index, int32 parent);
	void ReadMeshData(aiNode* node, int32 bone);
	void ReadSkinData();
	void WriteModelFile(wstring finalPath);

private:
	void ReadMaterialData();
	void WriteMaterialData(wstring finalPath);
	string WriteTexture(string saveFolder, string file);

private:
	shared_ptr<asAnimation> ReadAnimationData(aiAnimation* srcAnimation);
	shared_ptr<asAnimationNode> ParseAnimationNode(shared_ptr<asAnimation> animation, aiNodeAnim* srcNode);
	void ReadKeyframeData(shared_ptr<asAnimation> animation, aiNode* node, map<string, shared_ptr<asAnimationNode>>& cache);
	void WriteAnimationData(shared_ptr<asAnimation> animation, wstring finalPath);

private:
	uint32 GetBoneIndex(const string& name);

private:
	wstring _assetPath = L"../Resources/PrevConverted/";
	wstring _modelPath = L"../Resources/Assets/Models/";
	wstring _texturePath = L"../Resources/Assets/Textures/";

private:
	shared_ptr<Assimp::Importer> _importer;
	const aiScene* _scene;

private:
	vector<shared_ptr<asBone>> _bones;
	vector<shared_ptr<asMesh>> _meshes;
	vector<shared_ptr<asMaterial>> _materials;
};

