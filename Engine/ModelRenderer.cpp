#include "pch.h"
#include "ModelRenderer.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "MathUtils.h"
#include "BVH.h"

ModelRenderer::ModelRenderer(shared_ptr<Shader> shader)
	: Super(ComponentType::ModelRenderer), _shader(shader)
{

}

ModelRenderer::~ModelRenderer()
{

}

void ModelRenderer::OnInspectorGUI()
{
	Super::OnInspectorGUI();

	auto mats = _model->GetMaterials();
	ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

	for (int i = 0; i < mats.size(); i++)
	{
		auto& mat = mats[i];
		MaterialDesc& desc = mat->GetMaterialDesc();

		// ���͸��� ���
		if (ImGui::TreeNodeEx(("Material " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
	
			if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) {}
			if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) {}
			if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) {}
			if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) {}

			ImGui::NewLine();
		
			// Diffuse Map
			{
				ImGui::BeginGroup();
				ImGui::TextColored(color, "Diffuse");

				if (mat->GetDiffuseMap() != nullptr)
					ImGui::Image((void*)mat->GetDiffuseMap()->GetComPtr().Get(), ImVec2(75, 75));
				else
					ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));

				ImGui::EndGroup();
			}		

			ImGui::SameLine(0.f, -2.f); // ���� �ٿ� ��ġ

			// Normal Map
			{
				ImGui::BeginGroup();
				ImGui::TextColored(color, "Normal");

				if (mat->GetNormalMap() != nullptr)
					ImGui::Image((void*)mat->GetNormalMap()->GetComPtr().Get(), ImVec2(75, 75));
				else
					ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));

				ImGui::EndGroup();
			}
			ImGui::SameLine(); // ���� �ٿ� ��ġ

			// Specular Map
			{
				ImGui::BeginGroup();
				ImGui::TextColored(color, "Specular");

				if (mat->GetSpecularMap() != nullptr)
					ImGui::Image((void*)mat->GetSpecularMap()->GetComPtr().Get(), ImVec2(75, 75));

				else
					ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));
		
				ImGui::EndGroup();
			}
			ImGui::TreePop(); 
		}
	}
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
	}
}

void ModelRenderer::ThumbnailRender(shared_ptr<Camera> cam , shared_ptr<Light> light, shared_ptr<class InstancingBuffer>& buffer)
{
	_shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());
	_shader->PushLightData(light->GetLightDesc());

	PushData(0, light, buffer);
}

void ModelRenderer::RenderInstancing(int32 tech, shared_ptr<Shader> shader , Matrix V, Matrix P, shared_ptr<Light> light,  shared_ptr<InstancingBuffer>& buffer)
{
	if (_model == nullptr)
		return;

	//ssao shadow �� ������ ��찡 �־�, ���� �� ������Ű�� ���� ������ �߰�
	auto prevShader = _shader;

	if(shader)
		ChangeShader(shader);

	_shader->PushGlobalData(V, P);
		
	wstring wname = GetGameObject()->GetObjectName();

	PushData(tech, light, buffer );

	ChangeShader(prevShader);
}

void ModelRenderer::PushData(uint8 technique, shared_ptr<Light>& light, shared_ptr<class InstancingBuffer>& buffer)
{
	if (light)
		_shader->PushLightData(light->GetLightDesc());

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

		_shader->DrawIndexedInstanced(technique, _pass, mesh->indexBuffer->GetCount(), buffer->GetCount());
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

	//�޽ð� �ϳ� �� ���
	TransformBoundingBox();
	float dist = 0.f;

	if (_boundingBox.Intersects(ray.position, ray.direction, dist))
	{
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
	}

	return false;
}

void ModelRenderer::SetShadowMap(shared_ptr<Texture> tex)
{
	const auto& materials = _model->GetMaterials();
	for (auto& material : materials)
	{
		material->SetShadowMap(tex);
	}
}	

void ModelRenderer::SetSsaoMap(ComPtr<ID3D11ShaderResourceView> srv)
{
	const auto& materials = _model->GetMaterials();
	for (auto& material : materials)
	{
		material->SetSsaoMap(srv);
	}
}

void ModelRenderer::TransformBoundingBox()
{
	Matrix W = GetTransform()->GetWorldMatrix();

	Vec3 vMin = Vec3(MathUtils::INF, MathUtils::INF, MathUtils::INF);
	Vec3 vMax = Vec3(-MathUtils::INF, -MathUtils::INF, -MathUtils::INF);

	Vec3 corners[8];
	BoundingBox modelBox;
	modelBox.Center = Vec3(0.f,1.0f,0.f);
	modelBox.Extents = Vec3(1.0f,1.0f,1.0f);
	modelBox.GetCorners(corners);

	for (int i = 0; i < 8; ++i) {
		corners[i] = XMVector3TransformCoord(corners[i], W);
	}
	for (const Vec3& corner : corners) {
		vMin = ::XMVectorMin(vMin, corner);
		vMax = ::XMVectorMax(vMax, corner);
	}
	_boundingBox.Center = 0.5f * (vMin + vMax);
	_boundingBox.Extents = 0.5f * (vMax - vMin);

}
