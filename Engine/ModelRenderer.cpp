#include "pch.h"
#include "ModelRenderer.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "ShadowMap.h"
#include "MathUtils.h"

ModelRenderer::ModelRenderer(shared_ptr<Shader> shader)
	: Super(ComponentType::ModelRenderer), _shader(shader)
{

}

ModelRenderer::~ModelRenderer()
{

}

void ModelRenderer::SetModel(shared_ptr<Model> model)
{
	_model = model;
	ChangeShader(_shader);
}

void ModelRenderer::ChangeShader(shared_ptr<Shader> shader)
{
	_shader = shader;

	const auto& materials = _model->GetMaterials();
	for (auto& material : materials)
	{
		material->SetShader(shader);
		auto shadowMap = GRAPHICS->GetShadowMap();
		material->SetShadowMap(static_pointer_cast<Texture>(shadowMap));
	}
}

void ModelRenderer::PreRenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
	if (_model == nullptr)
		return;

	auto shader = RESOURCES->Get<Shader>(L"Shadow");
	ChangeShader(shader);

	_shader->PushGlobalData(Light::S_MatView, Light::S_MatProjection);

	PushData(buffer);
	
}

void ModelRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
	if (_model == nullptr)
		return;

	auto go = _gameObject.lock();

	if(go->GetUIPicked())
	{
		auto shader = RESOURCES->Get<Shader>(L"Outline");
		ChangeShader(shader);

		DCT->OMSetDepthStencilState(GRAPHICS->GetDSStateOutline().Get(), 1);

		auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
		// GlobalData
		_shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

		PushData(buffer);
	}

	{
		auto shader = RESOURCES->Get<Shader>(L"Standard");
		ChangeShader(shader);

		auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
		DCT->OMSetDepthStencilState(nullptr, 1);
	
		// GlobalData
		_shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

		PushData(buffer);
	}


}

void ModelRenderer::PushData(shared_ptr<class InstancingBuffer>& buffer)
{
	// Light
	auto lightObj = SCENE->GetCurrentScene()->GetLight();
	if (lightObj)
		_shader->PushLightData(lightObj->GetLight()->GetLightDesc());


	// Bones
	BoneDesc boneDesc;

	const uint32 boneCount = _model->GetBoneCount();
	for (uint32 i = 0; i < boneCount; i++)
	{
		shared_ptr<ModelBone> bone = _model->GetBoneByIndex(i);
		boneDesc.transforms[i] = bone->transform;
	}
	_shader->PushBoneData(boneDesc);

	const auto& meshes = _model->GetMeshes();
	for (auto& mesh : meshes)
	{
		if (mesh->material)	
			mesh->material->Update();
		
		// BoneIndex
		_shader->GetScalar("BoneIndex")->SetInt(mesh->boneIndex);

		// IA
		mesh->vertexBuffer->PushData();
		mesh->indexBuffer->PushData();

		buffer->PushData();

		_shader->DrawIndexedInstanced(0, _pass, mesh->indexBuffer->GetCount(), buffer->GetCount());
	}
}

InstanceID ModelRenderer::GetInstanceID()
{
	return make_pair((uint64)_model.get(), (uint64)_shader.get());
}

bool ModelRenderer::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();
	Matrix W = GetTransform()->GetWorldMatrix();

	Viewport& vp = GRAPHICS->GetViewport();

	Vec3 n = vp.Unproject(Vec3(screenX, screenY, 0), Matrix::Identity, V, P);
	Vec3 f = vp.Unproject(Vec3(screenX, screenY, 1), Matrix::Identity, V, P);
	Vec3 start = n;
	Vec3 direction = f - n;
	direction.Normalize();
	Ray ray = Ray(start, direction); 

	vector<shared_ptr<ModelMesh>>& meshes = _model->GetMeshes();
	vector<shared_ptr<ModelBone>>& bones = _model->GetBones();

	for (auto mesh : meshes)
	{
		Matrix boneWorldMatrix = bones[mesh->boneIndex]->transform * W;

		const auto& vertices = mesh->GetGeometry()->GetVertices();
		const auto& indices = mesh->GetGeometry()->GetIndices();

		for (uint32 i = 0; i < indices.size() / 3; ++i)
		{
			uint32 i0 = indices[i * 3 + 0];
			uint32 i1 = indices[i * 3 + 1];
			uint32 i2 = indices[i * 3 + 2];

			Vec3 v0 = XMVector3TransformCoord(vertices[i0].position, boneWorldMatrix);
			Vec3 v1 = XMVector3TransformCoord(vertices[i1].position, boneWorldMatrix);
			Vec3 v2 = XMVector3TransformCoord(vertices[i2].position, boneWorldMatrix);

			if (ray.Intersects(v0, v1, v2, OUT distance))
			{
				pickPos = ray.position + ray.direction * distance;
				return true;
			}
		}
	}

	return false;
}
