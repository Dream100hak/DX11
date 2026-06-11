#include "pch.h"
#include "UfbxConverter.h"
#include <filesystem>
#include "Utils.h"
#include "FileUtils.h"

namespace
{
	// ufbx 행렬(열 기준 3x4, 열벡터 규약) -> 엔진 행렬(행벡터 규약 v*M)
	Matrix ToMatrix(const ufbx_matrix& m)
	{
		return Matrix(
			(float)m.cols[0].x, (float)m.cols[0].y, (float)m.cols[0].z, 0.f,
			(float)m.cols[1].x, (float)m.cols[1].y, (float)m.cols[1].z, 0.f,
			(float)m.cols[2].x, (float)m.cols[2].y, (float)m.cols[2].z, 0.f,
			(float)m.cols[3].x, (float)m.cols[3].y, (float)m.cols[3].z, 1.f);
	}

	Vec3 ToVec3(const ufbx_vec3& v) { return Vec3((float)v.x, (float)v.y, (float)v.z); }

	Color ToColor(const ufbx_material_map& map, const Color& fallback)
	{
		if (map.has_value == false)
			return fallback;
		return Color((float)map.value_vec4.x, (float)map.value_vec4.y, (float)map.value_vec4.z, 1.f);
	}
}

UfbxConverter::UfbxConverter()
{
}

UfbxConverter::~UfbxConverter()
{
	if (_scene)
		ufbx_free_scene(_scene);
}

void UfbxConverter::ReadAssetFile(wstring file)
{
	// 절대 경로면 그대로, 아니면 PrevConverted 기준 상대 경로
	wstring fileStr = filesystem::path(file).is_absolute() ? file : _assetPath + file;

	auto p = std::filesystem::path(fileStr);
	assert(std::filesystem::exists(p));

	ufbx_load_opts opts = {};
	// D3D 좌수 좌표계(Y-up)로 변환 — Assimp 의 aiProcess_ConvertToLeftHanded 대응
	// (와인딩은 ufbx 가 자동으로 뒤집음 — handedness_conversion_retain_winding=false 기본값)
	opts.target_axes = ufbx_axes_left_handed_y_up;
	opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	// 단위 정규화 — Assimp 의 aiProcess_GlobalScale 대응 (FBX 단위 -> 미터)
	opts.target_unit_meters = 1.0f;
	// 단위/축 변환을 정점에 직접 베이크 — Assimp 산출물(정점이 미터 단위)과 스케일 일치
	// (ADJUST_TRANSFORMS 는 노드 스케일에 넣어 정점이 cm 로 남음 → 기존 에셋 대비 100배 커짐)
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	// 지오메트리 트랜스폼은 정점에 베이크 (엔진에 대응 개념 없음)
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
	opts.generate_missing_normals = true;

	ufbx_error error;
	_scene = ufbx_load_file(Utils::ToString(fileStr).c_str(), &opts, &error);

	if (_scene == nullptr)
	{
		string msg = error.description.data ? error.description.data : "unknown";
		assert(false && "ufbx_load_file failed");
	}
}

void UfbxConverter::ExportModelData(wstring savePath)
{
	wstring finalPath = _modelPath + savePath + L".mesh";

	_bones.clear();
	_meshes.clear();
	_nodeToBoneIndex.clear();

	ReadModelData(_scene->root_node, -1, -1);
	WriteModelFile(finalPath);
}

void UfbxConverter::ExportMaterialDataByMats(wstring savePath)
{
	ReadMaterialData();

	wstring finalPath = _modelPath + savePath + L".mmat";

	auto path = filesystem::path(finalPath);
	filesystem::create_directories(path.parent_path());
	string parentPath = path.parent_path().string() + "\\";

	// Material = .mat
	vector<wstring> matNames;
	for (shared_ptr<UfMaterial> material : _materials)
	{
		wstring matName = Utils::ToWString(parentPath + material->name);
		WriteMaterialDataByMat(material, matName);
		matNames.push_back(matName);
	}

	// Model Material = .mmat
	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(finalPath, FileMode::Write);

	uint32 size = (uint32)_materials.size();

	file->Write<uint32>(size);
	for (auto& matName : matNames)
	{
		file->Write<string>(Utils::ToString(matName));
	}
}

void UfbxConverter::ExportAnimationData(wstring savePath, uint32 index /*= 0*/)
{
	wstring finalPath = _modelPath + savePath + L".clip";
	assert(index < _scene->anim_stacks.count);

	ufbx_anim_stack* stack = _scene->anim_stacks.data[index];

	float fps = (float)_scene->settings.frames_per_second;
	if (fps <= 0.f)
		fps = 30.f;

	double timeBegin = stack->time_begin;
	double timeEnd = stack->time_end;

	shared_ptr<asAnimation> animation = make_shared<asAnimation>();
	animation->name = stack->name.data;
	animation->frameRate = fps;
	animation->frameCount = (uint32)(::round((timeEnd - timeBegin) * fps)) + 1;
	animation->duration = (float)(animation->frameCount - 1); // 틱 단위 (frameRate 와 함께 해석)

	// 본 트리 순회 순서와 동일하게 모든 노드의 로컬 트랜스폼을 프레임별로 베이크
	BakeKeyframes(animation, stack->anim, _scene->root_node, timeBegin);

	WriteAnimationData(animation, finalPath);
}

void UfbxConverter::ReadModelData(ufbx_node* node, int32 index, int32 parent)
{
	shared_ptr<asBone> bone = make_shared<asBone>();
	bone->index = index;
	bone->parent = parent;
	bone->name = node->name.data;

	// 로컬 -> 모델 공간 누적 (AsConverter 와 동일한 누적 방식)
	Matrix local = ToMatrix(node->node_to_parent);

	Matrix matParent = Matrix::Identity;
	if (parent >= 0)
		matParent = _bones[parent]->transform;

	bone->transform = local * matParent;

	_bones.push_back(bone);
	_nodeToBoneIndex[node] = index;

	// Mesh
	ReadMeshData(node, index);

	// 자식 노드 반복
	for (size_t i = 0; i < node->children.count; i++)
		ReadModelData(node->children.data[i], (int32)_bones.size(), index);
}

void UfbxConverter::ReadMeshData(ufbx_node* node, int32 boneIndex)
{
	if (node->mesh == nullptr)
		return;

	ufbx_mesh* srcMesh = node->mesh;

	shared_ptr<asMesh> mesh = make_shared<asMesh>();
	mesh->name = node->name.data;
	mesh->boneIndex = boneIndex;

	// 머티리얼 이름 (인스턴스별 머티리얼 우선)
	if (node->materials.count > 0)
		mesh->materialName = node->materials.data[0]->name.data;
	else if (srcMesh->materials.count > 0)
		mesh->materialName = srcMesh->materials.data[0]->name.data;

	ufbx_skin_deformer* skin = srcMesh->skin_deformers.count > 0 ? srcMesh->skin_deformers.data[0] : nullptr;

	bool hasTangent = srcMesh->vertex_tangent.exists;

	// AABB 누적
	Vec3 aabbMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 aabbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// 삼각분할 후 플랫 정점 생성 (인덱스 = 순번)
	vector<uint32> triIndices(srcMesh->max_face_triangles * 3);

	for (size_t f = 0; f < srcMesh->faces.count; f++)
	{
		ufbx_face face = srcMesh->faces.data[f];
		uint32 numTris = ufbx_triangulate_face(triIndices.data(), triIndices.size(), srcMesh, face);

		for (uint32 i = 0; i < numTris * 3; i++)
		{
			uint32 ix = triIndices[i];

			VertexType vertex = {};

			ufbx_vec3 pos = ufbx_get_vertex_vec3(&srcMesh->vertex_position, ix);
			vertex.position = ToVec3(pos);

			if (srcMesh->vertex_uv.exists)
			{
				ufbx_vec2 uv = ufbx_get_vertex_vec2(&srcMesh->vertex_uv, ix);
				// FBX UV 원점(좌하단) -> D3D(좌상단) — Assimp aiProcess_FlipUVs 와 동일 (산출물 바이트 일치 확인됨)
				vertex.uv = Vec2((float)uv.x, 1.f - (float)uv.y);
			}

			if (srcMesh->vertex_normal.exists)
				vertex.normal = ToVec3(ufbx_get_vertex_vec3(&srcMesh->vertex_normal, ix));

			if (hasTangent)
				vertex.tangent = ToVec3(ufbx_get_vertex_vec3(&srcMesh->vertex_tangent, ix));

			// 스킨 가중치 (원본 정점 ID 기준, 상위 4개 + 정규화)
			if (skin)
			{
				uint32 vid = srcMesh->vertex_indices.data[ix];

				asBoneWeights boneWeights;
				const ufbx_skin_vertex& sv = skin->vertices.data[vid];
				for (uint32 w = 0; w < sv.num_weights; w++)
				{
					const ufbx_skin_weight& sw = skin->weights.data[sv.weight_begin + w];
					ufbx_node* boneNode = skin->clusters.data[sw.cluster_index]->bone_node;
					if (boneNode == nullptr)
						continue;

					// 노드 포인터로 본 인덱스 조회 (이름 매칭은 중복/빈 이름에서 깨짐)
					auto it = _nodeToBoneIndex.find(boneNode);
					if (it == _nodeToBoneIndex.end() || it->second < 0)
						continue;

					boneWeights.AddWeights((uint32)it->second, (float)sw.weight);
				}

				if (boneWeights.boneWeights.empty() == false)
				{
					boneWeights.Normalize();
					asBlendWeight blendWeight = boneWeights.GetBlendWeights();
					vertex.blendIndices = blendWeight.indices;
					vertex.blendWeights = blendWeight.weights;
				}
			}

			aabbMin = Vec3::Min(aabbMin, vertex.position);
			aabbMax = Vec3::Max(aabbMax, vertex.position);

			mesh->indices.push_back((uint32)mesh->vertices.size());
			mesh->vertices.push_back(vertex);
		}
	}

	mesh->aabb.min = aabbMin;
	mesh->aabb.max = aabbMax;

	// 탄젠트가 없으면 UV 기반으로 생성 (노멀맵 셰이딩 품질 확보)
	if (hasTangent == false && srcMesh->vertex_uv.exists)
		GenerateTangents(mesh);

	_meshes.push_back(mesh);
}

// UV 공간 기반 삼각형별 탄젠트 누적 후 Gram-Schmidt 직교화
void UfbxConverter::GenerateTangents(shared_ptr<asMesh> mesh)
{
	vector<Vec3> accum(mesh->vertices.size(), Vec3::Zero);

	for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3)
	{
		uint32 i0 = mesh->indices[i + 0];
		uint32 i1 = mesh->indices[i + 1];
		uint32 i2 = mesh->indices[i + 2];

		const VertexType& v0 = mesh->vertices[i0];
		const VertexType& v1 = mesh->vertices[i1];
		const VertexType& v2 = mesh->vertices[i2];

		Vec3 e1 = v1.position - v0.position;
		Vec3 e2 = v2.position - v0.position;
		Vec2 duv1 = v1.uv - v0.uv;
		Vec2 duv2 = v2.uv - v0.uv;

		float det = duv1.x * duv2.y - duv2.x * duv1.y;
		if (::fabsf(det) < 1e-8f)
			continue;

		float r = 1.f / det;
		Vec3 tangent = (e1 * duv2.y - e2 * duv1.y) * r;

		accum[i0] += tangent;
		accum[i1] += tangent;
		accum[i2] += tangent;
	}

	for (size_t v = 0; v < mesh->vertices.size(); v++)
	{
		Vec3 n = mesh->vertices[v].normal;
		Vec3 t = accum[v];

		// 노멀 성분 제거 후 정규화
		t = t - n * n.Dot(t);
		float len = t.Length();
		if (len > 1e-6f)
			mesh->vertices[v].tangent = t / len;
		else
			mesh->vertices[v].tangent = Vec3(1.f, 0.f, 0.f);
	}
}

void UfbxConverter::WriteModelFile(wstring finalPath)
{
	auto path = filesystem::path(finalPath);
	filesystem::create_directories(path.parent_path());

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(finalPath, FileMode::Write);

	// Bone Data — AsConverter 와 동일 포맷
	file->Write<uint32>((uint32)_bones.size());
	for (shared_ptr<asBone>& bone : _bones)
	{
		file->Write<int32>(bone->index);
		file->Write<string>(bone->name);
		file->Write<int32>(bone->parent);
		file->Write<Matrix>(bone->transform);
	}

	// Mesh Data
	file->Write<uint32>((uint32)_meshes.size());
	for (shared_ptr<asMesh>& meshData : _meshes)
	{
		file->Write<string>(meshData->name);
		file->Write<int32>(meshData->boneIndex);
		file->Write<string>(meshData->materialName);

		// Vertex Data
		file->Write<uint32>((uint32)meshData->vertices.size());
		file->Write(&meshData->vertices[0], (uint32)(sizeof(VertexType) * meshData->vertices.size()));

		// Index Data
		file->Write<uint32>((uint32)meshData->indices.size());
		file->Write(&meshData->indices[0], (uint32)(sizeof(uint32) * meshData->indices.size()));

		file->Write<MeshAabb>(meshData->aabb);
	}
}

void UfbxConverter::ReadMaterialData()
{
	_materials.clear();

	for (size_t i = 0; i < _scene->materials.count; i++)
	{
		ufbx_material* sm = _scene->materials.data[i];

		shared_ptr<UfMaterial> material = make_shared<UfMaterial>();
		material->name = sm->name.data;

		material->ambient = ToColor(sm->fbx.ambient_color, Color(1.f, 1.f, 1.f, 1.f));
		material->diffuse = ToColor(sm->fbx.diffuse_color, ToColor(sm->pbr.base_color, Color(1.f, 1.f, 1.f, 1.f)));
		material->specular = ToColor(sm->fbx.specular_color, Color(0.f, 0.f, 0.f, 1.f));
		if (sm->fbx.specular_exponent.has_value)
			material->specular.w = (float)sm->fbx.specular_exponent.value_real;
		material->emissive = ToColor(sm->fbx.emission_color, Color(0.f, 0.f, 0.f, 1.f));

		// PBR 추출 (기존 Assimp 경로는 0.5/0.0 고정이었음)
		if (sm->pbr.roughness.has_value)
			material->roughness = (float)sm->pbr.roughness.value_real;
		if (sm->pbr.metalness.has_value)
			material->metallic = (float)sm->pbr.metalness.value_real;

		// 텍스처 (PBR base color 우선, FBX 클래식 폴백)
		material->diffuseTex = sm->pbr.base_color.texture ? sm->pbr.base_color.texture : sm->fbx.diffuse_color.texture;
		material->specularTex = sm->fbx.specular_color.texture;
		material->normalTex = sm->fbx.normal_map.texture ? sm->fbx.normal_map.texture : sm->pbr.normal_map.texture;

		_materials.push_back(material);
	}
}

void UfbxConverter::WriteMaterialDataByMat(shared_ptr<UfMaterial> material, wstring finalPath)
{
	wstring fullPath = finalPath + L".mat";
	auto path = filesystem::path(fullPath);
	filesystem::create_directories(path.parent_path());
	string folder = path.parent_path().string();

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Write);

	// .mat 포맷 호환: 셰이더 문자열 헤더 (로더는 문자열만 읽고 Standard_HLSL 바인딩)
	file->Write<string>(string("01. Standard.fx"));
	file->Write<string>(WriteTexture(folder, material->diffuseTex));
	file->Write<string>(WriteTexture(folder, material->specularTex));
	file->Write<string>(WriteTexture(folder, material->normalTex));
	file->Write<Color>(material->ambient);
	file->Write<Color>(material->diffuse);
	file->Write<Color>(material->specular);
	file->Write<Color>(material->emissive);
	// PBR — FBX 에서 추출한 실제 값
	file->Write<float>(material->roughness);
	file->Write<float>(material->metallic);
}

std::string UfbxConverter::WriteTexture(string saveFolder, ufbx_texture* texture)
{
	if (texture == nullptr)
		return "";

	string srcName = texture->relative_filename.length > 0
		? string(texture->relative_filename.data)
		: string(texture->filename.data);

	string fileName = filesystem::path(srcName).filename().string();
	if (fileName.empty())
		return "";

	string pathStr = (filesystem::path(saveFolder) / fileName).string();

	if (texture->content.size > 0)
	{
		// 임베디드 텍스처 — 원본 파일 바이트 그대로 저장 (png/jpg 등)
		shared_ptr<FileUtils> file = make_shared<FileUtils>();
		file->Open(Utils::ToWString(pathStr), FileMode::Write);
		file->Write((void*)texture->content.data, (uint32)texture->content.size);
	}
	else
	{
		// 외부 참조 — ufbx 가 해석한 절대 경로에서 복사
		string originStr = texture->filename.length > 0 ? string(texture->filename.data) : srcName;
		::CopyFileA(originStr.c_str(), pathStr.c_str(), false);
	}

	return fileName;
}

void UfbxConverter::BakeKeyframes(shared_ptr<asAnimation> animation, ufbx_anim* anim, ufbx_node* node, double timeBegin)
{
	shared_ptr<asKeyframe> keyframe = make_shared<asKeyframe>();
	keyframe->boneName = node->name.data;

	const double invFps = 1.0 / (double)animation->frameRate;

	for (uint32 f = 0; f < animation->frameCount; f++)
	{
		double time = timeBegin + (double)f * invFps;
		ufbx_transform tr = ufbx_evaluate_transform(anim, node, time);

		asKeyframeData frameData;
		frameData.time = (float)f;
		frameData.scale = ToVec3(tr.scale);
		frameData.rotation = Quaternion((float)tr.rotation.x, (float)tr.rotation.y, (float)tr.rotation.z, (float)tr.rotation.w);
		frameData.translation = ToVec3(tr.translation);

		keyframe->transforms.push_back(frameData);
	}

	animation->keyframes.push_back(keyframe);

	for (size_t i = 0; i < node->children.count; i++)
		BakeKeyframes(animation, anim, node->children.data[i], timeBegin);
}

void UfbxConverter::WriteAnimationData(shared_ptr<asAnimation> animation, wstring finalPath)
{
	auto path = filesystem::path(finalPath);
	filesystem::create_directories(path.parent_path());

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(finalPath, FileMode::Write);

	file->Write<string>(animation->name);
	file->Write<float>(animation->duration);
	file->Write<float>(animation->frameRate);
	file->Write<uint32>(animation->frameCount);

	file->Write<uint32>((uint32)animation->keyframes.size());

	for (shared_ptr<asKeyframe> keyframe : animation->keyframes)
	{
		file->Write<string>(keyframe->boneName);

		file->Write<uint32>((uint32)keyframe->transforms.size());
		file->Write(&keyframe->transforms[0], (uint32)(sizeof(asKeyframeData) * keyframe->transforms.size()));
	}
}

uint32 UfbxConverter::GetBoneIndex(const string& name)
{
	for (shared_ptr<asBone>& bone : _bones)
	{
		if (bone->name == name)
			return bone->index;
	}

	assert(false);
	return 0;
}
