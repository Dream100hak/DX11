#include "pch.h"
#include "Model.h"
#include "Utils.h"
#include "FileUtils.h"
#include "tinyxml2.h"
#include <filesystem>
#include "Material.h"
#include "ModelMesh.h"
#include "ModelAnimation.h"
#include "MathUtils.h"

Model::Model() : Super(ResourceType::Model)
{

}

Model::~Model()
{

}

void Model::ReadMaterial(wstring filename)
{
	wstring fullPath = _modelPath + filename + L".mmat";
	auto parentPath = filesystem::path(fullPath).parent_path();
	
	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Read);

	int32 size =  file->Read<int32>();

	for (int32 i = 0; i < size; i++)
	{
		string matName = file->Read<string>();

		// 정규화 키 — 프로젝트 창(.mat 스캔)과 같은 인스턴스를 공유해야
		// 인스펙터에서 .mat 을 고치면 이 모델도 같이 바뀐다
		wstring matKey = Utils::ToMaterialKey(Utils::ToWString(matName));
		shared_ptr<Material> material = RESOURCES->Get<Material>(matKey);
		if (material == nullptr)
		{
			material = make_shared<Material>();
			material->Load(Utils::ToWString(matName));
			RESOURCES->Add(matKey, material);
		}
			
		_materials.push_back(material);
	}

	BindCacheInfo();
}

void Model::ReadMaterialByXml(wstring filename)
{
	wstring fullPath = _texturePath + filename + L".xml";
	auto parentPath = filesystem::path(fullPath).parent_path();

	tinyxml2::XMLDocument* document = new tinyxml2::XMLDocument();
	tinyxml2::XMLError error = document->LoadFile(Utils::ToString(fullPath).c_str());
	assert(error == tinyxml2::XML_SUCCESS);

	tinyxml2::XMLElement* root = document->FirstChildElement();
	tinyxml2::XMLElement* materialNode = root->FirstChildElement();

	while (materialNode)
	{
		shared_ptr<Material> material = make_shared<Material>();
		tinyxml2::XMLElement* node = nullptr;

		node = materialNode->FirstChildElement();
		material->SetName(Utils::ToWString(node->GetText()));

		// Diffuse Texture
		node = node->NextSiblingElement();
		if (node->GetText())
		{
			wstring textureStr = Utils::ToWString(node->GetText());
			if (textureStr.length() > 0)
			{
				auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
				material->SetDiffuseMap(texture);
			}
		}
		// Specular Texture
		node = node->NextSiblingElement();
		if (node->GetText())
		{
			wstring texture = Utils::ToWString(node->GetText());
			if (texture.length() > 0)
			{
				wstring textureStr = Utils::ToWString(node->GetText());
				if (textureStr.length() > 0)
				{
					auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
					material->SetSpecularMap(texture);
				}
			}
		}

		// Normal Texture
		node = node->NextSiblingElement();
		if (node->GetText())
		{
			wstring textureStr = Utils::ToWString(node->GetText());
			if (textureStr.length() > 0)
			{
				auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
				material->SetNormalMap(texture);
			}
		}

		// Ambient
		{
			node = node->NextSiblingElement();

			Color color;
			color.x = node->FloatAttribute("R");
			color.y = node->FloatAttribute("G");
			color.z = node->FloatAttribute("B");
			color.w = node->FloatAttribute("A");
			material->GetMaterialDesc().ambient = color;
		}

		// Diffuse
		{
			node = node->NextSiblingElement();

			Color color;
			color.x = node->FloatAttribute("R");
			color.y = node->FloatAttribute("G");
			color.z = node->FloatAttribute("B");
			color.w = node->FloatAttribute("A");
			material->GetMaterialDesc().diffuse = color;
		}

		// Specular
		{
			node = node->NextSiblingElement();

			Color color;
			color.x = node->FloatAttribute("R");
			color.y = node->FloatAttribute("G");
			color.z = node->FloatAttribute("B");
			color.w = node->FloatAttribute("A");
			material->GetMaterialDesc().specular = color;
		}

		// Emissive
		{
			node = node->NextSiblingElement();

			Color color;
			color.x = node->FloatAttribute("R");
			color.y = node->FloatAttribute("G");
			color.z = node->FloatAttribute("B");
			color.w = node->FloatAttribute("A");
			material->GetMaterialDesc().emissive = color;
		}

		_materials.push_back(material);

		// Next Material
		materialNode = materialNode->NextSiblingElement();
	}

	BindCacheInfo();
}

void Model::ReadModel(wstring filename)
{
	_relativePath = filename; // 씬 직렬화용 로드 키 보존

	wstring fullPath = _modelPath + filename + L".mesh";

	auto path = filesystem::path(fullPath);
	SetName(path.filename().wstring());

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Read);

	// Bones
	{
		const uint32 count = file->Read<uint32>();

		for (uint32 i = 0; i < count; i++)
		{
			shared_ptr<ModelBone> bone = make_shared<ModelBone>();
			bone->index = file->Read<int32>();
			bone->name = Utils::ToWString(file->Read<string>());
			bone->parentIndex = file->Read<int32>();
			bone->transform = file->Read<Matrix>();

			_bones.push_back(bone);
		}
	}

	// Mesh
	{
		const uint32 count = file->Read<uint32>();

		for (uint32 i = 0; i < count; i++)
		{
			shared_ptr<ModelMesh> mesh = make_shared<ModelMesh>();

			mesh->name = Utils::ToWString(file->Read<string>());
			mesh->boneIndex = file->Read<int32>();

			// Material
			mesh->materialName = Utils::ToWString(file->Read<string>());

			//VertexData
			{
				const uint32 count = file->Read<uint32>();
				vector<ModelVertexType> vertices;
				vertices.resize(count);

				void* data = vertices.data();
				file->Read(&data, sizeof(ModelVertexType) * count);
				mesh->geometry->AddVertices(vertices);
			}

			//IndexData
			{
				const uint32 count = file->Read<uint32>();

				vector<uint32> indices;
				indices.resize(count);

				void* data = indices.data();
				file->Read(&data, sizeof(uint32) * count);
				mesh->geometry->AddIndices(indices);
			}

			//ABB	
			mesh->aabb = file->Read<MeshAabb>();
		
			mesh->CreateBuffers();
			_meshes.push_back(mesh);
		}
	}

	BindCacheInfo();
}

void Model::ReadAnimation(wstring filename)
{
	wstring fullPath = _modelPath + filename + L".clip";
	auto path = filesystem::path(fullPath);

	shared_ptr<FileUtils> file = make_shared<FileUtils>();
	file->Open(fullPath, FileMode::Read);

	shared_ptr<ModelAnimation> animation = make_shared<ModelAnimation>();

	animation->fileName = Utils::ToWString(path.filename().string());
	animation->clipName = Utils::ToWString(file->Read<string>());
	animation->duration = file->Read<float>();
	animation->frameRate = file->Read<float>();
	animation->frameCount = file->Read<uint32>();

	uint32 keyframesCount = file->Read<uint32>();

	for (uint32 i = 0; i < keyframesCount; i++)
	{
		shared_ptr<ModelKeyframe> keyframe = make_shared<ModelKeyframe>();
		keyframe->boneName = Utils::ToWString(file->Read<string>());

		uint32 size = file->Read<uint32>();

		if (size > 0)
		{
			keyframe->transforms.resize(size);
			void* ptr = &keyframe->transforms[0];
			file->Read(&ptr, sizeof(ModelKeyframeData) * size);
		}

		animation->keyframes[keyframe->boneName] = keyframe;
	}

	_animations.push_back(animation);
}

std::shared_ptr<Material> Model::GetMaterialByName(const wstring& name)
{
	for (auto& material : _materials)
	{
		if (material->GetName() == name)
			return material;

		// .mmat 경로(Material::Load)로 로드된 머티리얼은 이름이 전체 경로(...\name.mat) —
		// 메시의 materialName(짧은 이름)과 매칭되도록 파일명 스템 비교 폴백
		// (이게 없으면 mesh->material 이 null 이 되어 이전 패스 SRV 를 샘플링하는 렌더 깨짐 발생)
		if (filesystem::path(material->GetName()).stem().wstring() == name)
			return material;
	}

	return nullptr;
}

std::shared_ptr<ModelMesh> Model::GetMeshByName(const wstring& name)
{
	for (auto& mesh : _meshes)
	{
		if (mesh->name == name)
			return mesh;
	}

	return nullptr;
}

std::shared_ptr<ModelBone> Model::GetBoneByName(const wstring& name)
{
	for (auto& bone : _bones)
	{
		if (bone->name == name)
			return bone;
	}

	return nullptr;
}

shared_ptr<ModelAnimation> Model::GetAnimationByFileName(wstring name)
{
	for (auto& animation : _animations)
	{
		if (animation->fileName == name)
			return animation;
	}

	return nullptr;
}

std::shared_ptr<ModelAnimation> Model::GetAnimationByClipName(wstring name)
{
	for (auto& animation : _animations)
	{
		if (animation->clipName == name)
			return animation;
	}

	return nullptr;
}

int32 Model::GetAnimIndexByFileName(wstring name)
{
	for (int32 i = 0; i < _animations.size(); ++i)
	{
		if (_animations[i]->fileName == name)
			return i;
	}

	return -1;
}

int32 Model::GetAnimIndexByClipName(wstring name)
{
	for (int32 i = 0; i < _animations.size(); ++i)
	{
		if (_animations[i]->clipName == name)
			return i;
	}

	return -1;
}

void Model::BindCacheInfo()
{
	// Mesh에 Material 캐싱
	for (const auto& mesh : _meshes)
	{
		// 이미 찾았으면 스킵
		if (mesh->material != nullptr)
			continue;

		mesh->material = GetMaterialByName(mesh->materialName);
	}

	// Mesh에 Bone 캐싱
	for (const auto& mesh : _meshes)
	{
		// 이미 찾았으면 스킵
		if (mesh->bone != nullptr)
			continue;

		mesh->bone = GetBoneByIndex(mesh->boneIndex);
	}

	// Bone 계층 정보 채우기
	if (_root == nullptr && _bones.size() > 0)
	{
		_root = _bones[0];

		for (const auto& bone : _bones)
		{
			if (bone->parentIndex >= 0)
			{
				bone->parent = _bones[bone->parentIndex];
				bone->parent->children.push_back(bone);
			}
			else
			{
				bone->parent = nullptr;
			}
		}
	}
}

DirectX::BoundingBox Model::CalculateModelBoundingBox()
{
	//XMFLOAT3 minPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	//XMFLOAT3 maxPoint = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	//
	//// 모든 메시의 정점을 순회하여 AABB 계산
	//for (const auto& mesh : _meshes) {
	//	for (const auto& vertex : mesh->geometry->GetVertices()) {
	//		minPoint.x = min(minPoint.x, vertex.position.x);
	//		minPoint.y = min(minPoint.y, vertex.position.y);
	//		minPoint.z = min(minPoint.z, vertex.position.z);

	//		maxPoint.x = max(maxPoint.x, vertex.position.x);
	//		maxPoint.y = max(maxPoint.y, vertex.position.y);
	//		maxPoint.z = max(maxPoint.z, vertex.position.z);
	//	}
	//}

	//// AABB 중심과 크기 계산
	//XMFLOAT3 center = XMFLOAT3(
	//	(minPoint.x + maxPoint.x) / 2.0f,
	//	(minPoint.y + maxPoint.y) / 2.0f,
	//	(minPoint.z + maxPoint.z) / 2.0f);

	//XMFLOAT3 extents = XMFLOAT3(
	//	(maxPoint.x - minPoint.x) / 2.0f,
	//	(maxPoint.y - minPoint.y) / 2.0f,
	//	(maxPoint.z - minPoint.z) / 2.0f);

	//BoundingBox aabb;
	//aabb.Center = center;
	//aabb.Extents = extents;

	//return aabb;

	XMFLOAT3 minPoint(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// 모든 메시의 AABB를 순회하여 AABB 계산
	for (const auto& mesh : _meshes) {
		const Vec3& aabbMin = mesh->aabb.min;
		const Vec3& aabbMax = mesh->aabb.max;

		minPoint.x = min(minPoint.x, aabbMin.x);
		minPoint.y = min(minPoint.y, aabbMin.y);
		minPoint.z = min(minPoint.z, aabbMin.z);

		maxPoint.x = max(maxPoint.x, aabbMax.x);
		maxPoint.y = max(maxPoint.y, aabbMax.y);
		maxPoint.z = max(maxPoint.z, aabbMax.z);
	}

	// AABB 중심과 크기 계산
	XMFLOAT3 center(
		(minPoint.x + maxPoint.x) / 2.0f,
		(minPoint.y + maxPoint.y) / 2.0f,
		(minPoint.z + maxPoint.z) / 2.0f
	);

	XMFLOAT3 extents(
		(maxPoint.x - minPoint.x) / 2.0f,
		(maxPoint.y - minPoint.y) / 2.0f,
		(maxPoint.z - minPoint.z) / 2.0f
	);

	BoundingBox aabb;
	BoundingBox::CreateFromPoints(aabb, XMLoadFloat3(&minPoint), XMLoadFloat3(&maxPoint));

	return aabb;
}
