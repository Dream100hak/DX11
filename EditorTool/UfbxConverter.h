#pragma once
#include "AsType.h"
#include <ufbx/ufbx.h>

// -----------------------------------------------------------
// UfbxConverter
//  - Assimp(AsConverter) 를 대체하는 ufbx 기반 FBX 변환기
//  - 출력 포맷(.mesh/.clip/.mat/.mmat)은 AsConverter 와 바이트 호환 (엔진 로더 수정 불필요)
//  - 개선점:
//    * FBX 에 탄젠트가 있으면 그대로 사용, 없으면 UV 기반으로 직접 생성 (Assimp NaN 문제 해결)
//    * PBR roughness/metallic 추출 (.mat 에 실제 값 기록, 기존엔 0.5/0.0 고정)
//    * 애니메이션을 ufbx_evaluate_transform 으로 베이크 (커브 타입 무관, 틱 매칭 휴리스틱 제거)
// -----------------------------------------------------------

struct UfMaterial
{
	string name;
	Color ambient  = Color(1.f, 1.f, 1.f, 1.f);
	Color diffuse  = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(0.f, 0.f, 0.f, 1.f);
	Color emissive = Color(0.f, 0.f, 0.f, 1.f);
	float roughness = 0.5f;
	float metallic  = 0.f;

	ufbx_texture* diffuseTex = nullptr;
	ufbx_texture* specularTex = nullptr;
	ufbx_texture* normalTex = nullptr;
};

class UfbxConverter
{
public:
	UfbxConverter();
	~UfbxConverter();

public:
	void ReadAssetFile(wstring file);
	void ExportModelData(wstring savePath);
	void ExportMaterialDataByMats(wstring savePath);
	void ExportAnimationData(wstring savePath, uint32 index = 0);

	uint32 GetAnimationCount() const { return _scene ? (uint32)_scene->anim_stacks.count : 0; }

private:
	// Model / Mesh / Skin
	void ReadModelData(ufbx_node* node, int32 index, int32 parent);
	void ReadMeshData(ufbx_node* node, int32 boneIndex);
	void GenerateTangents(shared_ptr<asMesh> mesh);
	void WriteModelFile(wstring finalPath);

	// Material
	void ReadMaterialData();
	void WriteMaterialDataByMat(shared_ptr<UfMaterial> material, wstring finalPath);
	string WriteTexture(string saveFolder, ufbx_texture* texture);

	// Animation
	void BakeKeyframes(shared_ptr<asAnimation> animation, ufbx_anim* anim, ufbx_node* node, double timeBegin);
	void WriteAnimationData(shared_ptr<asAnimation> animation, wstring finalPath);

	uint32 GetBoneIndex(const string& name);

private:
	wstring _assetPath = L"../Resources/PrevConverted/";
	wstring _modelPath = L"../Resources/Assets/Models/";

private:
	ufbx_scene* _scene = nullptr;

	// 노드 포인터 -> 본 인덱스 (이름 중복/빈 이름에 안전한 스킨 본 매핑)
	unordered_map<const ufbx_node*, int32> _nodeToBoneIndex;

	vector<shared_ptr<asBone>> _bones;
	vector<shared_ptr<asMesh>> _meshes;
	vector<shared_ptr<UfMaterial>> _materials;
};
