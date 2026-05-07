#include "pch.h"
#include "ModelRenderer.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "MathUtils.h"
#include "Utils.h"
#include "RenderContext.h"

ModelRenderer::ModelRenderer(shared_ptr<Shader> shader)
	: Super(RendererType::Model), _shader(shader)
{

}

ModelRenderer::~ModelRenderer()
{

}

void ModelRenderer::OnInspectorGUI()
{
	Super::OnInspectorGUI();

	string modelName = Utils::ToString(_model->GetName());

	ImGui::Dummy(ImVec2(0, 20.f));

	string modelPanel = "Model";
	ImGui::Text(modelPanel.c_str());

	ImGui::SameLine();
	float space = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(modelPanel.c_str()).x - ImGui::GetStyle().ItemSpacing.x;
	ImGui::Dummy(ImVec2(space - 80, 0)); // Fill the space
	ImGui::SameLine();
	
	// Push style color and variable to customize button
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));  // Gray color
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Slightly lighter gray when hovered
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));  // Slightly darker gray when clicked
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);  // Rounded corners
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));  // Padding inside button

	ImGui::Button(modelName.c_str());

	// Pop style color and variable to revert to default
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);

	auto mats = _model->GetMaterials();
	ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

	for (int i = 0; i < mats.size(); i++)
	{

		auto& mat = mats[i];
		MaterialDesc& desc = mat->GetMaterialDesc();

		// 매터리얼 노드
		if (ImGui::TreeNodeEx( Utils::ToString(mat->GetName()).c_str() , ImGuiTreeNodeFlags_DefaultOpen))
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

			ImGui::SameLine(0.f, -2.f); // 같은 줄에 배치

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
			ImGui::SameLine(); // 같은 줄에 배치

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
		material->SetShadowMap(_shadowMap);
		material->SetSsaoMap(_ssaoMap);
	}
}

// ============================================================
// Draw() ? 단일 진입점
// ============================================================
void ModelRenderer::Draw(const RenderContext& ctx)
{
	if (_model == nullptr)
		return;

	auto prevShader = _shader;
	if (ctx.shaderOverride)
		ChangeShader(ctx.shaderOverride);

	_shader->PushGlobalData(ctx.view, ctx.proj);
	if (ctx.light) _shader->PushLightData(ctx.light->GetLightDesc());

	PushMeshes(ctx, ctx.buffer != nullptr);

	ChangeShader(prevShader);
}

// 메시 루프 (Single / Instanced 공통)
void ModelRenderer::PushMeshes(const RenderContext& ctx, bool instanced)
{
	// Bone 데이터
	BoneDesc boneDesc;
	const uint32 boneCount = _model->GetBoneCount();
	for (uint32 i = 0; i < boneCount; i++)
		boneDesc.transforms[i] = _model->GetBoneByIndex(i)->transform;
	_shader->PushBoneData(boneDesc);

	if (!instanced)
		_shader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

	const auto& meshes = _model->GetMeshes();
	for (auto& mesh : meshes)
	{
		if (mesh->material) mesh->material->Update();
		_shader->GetScalar("BoneIndex")->SetInt(mesh->boneIndex);

		mesh->vertexBuffer->PushData();
		mesh->indexBuffer->PushData();

		if (!instanced)
			_shader->DrawIndexed(0, _pass, mesh->indexBuffer->GetCount(), 0, 0);
		else
		{
			ctx.buffer->PushData();
			_shader->DrawIndexedInstanced(ctx.tech, _pass, mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
		}
	}
}

InstanceID ModelRenderer::GetInstanceID()
{
	return make_pair((uint64)_model.get(), (uint64)_shader.get());
}

bool ModelRenderer::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
	Super::Pick(screenX, screenY, pickPos, distance);

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

	//메시가 하나 일 경우
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

