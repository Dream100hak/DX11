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
#include "HlslShader.h"
#include "RenderStateManager.h"
#include "Texture.h"

ModelRenderer::ModelRenderer()
	: Super(RendererType::Model)
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

		// 占쏙옙占싶몌옙占쏙옙 占쏙옙占?
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

			ImGui::SameLine(0.f, -2.f); // 占쏙옙占쏙옙 占쌕울옙 占쏙옙치

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
			ImGui::SameLine(); // 占쏙옙占쏙옙 占쌕울옙 占쏙옙치

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
}

// ============================================================
// Draw() ? 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙
// ============================================================
void ModelRenderer::Draw(const RenderContext& ctx)
{
	if (_model == nullptr)
		return;

	// ?? Deferred G-Buffer 寃쎈줈 (HLSL, ?뺤쟻 紐⑤뜽) ??
	if (ctx.deferredPass)
	{
		auto gbuf = RESOURCES->Get<HlslShader>(L"GBufferModel_HLSL");
		if (!gbuf) return;

		gbuf->Bind();
		gbuf->PushGlobalData(ctx.view, ctx.proj);
		if (!ctx.buffer)
			gbuf->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

		const auto& meshes = _model->GetMeshes();
		for (auto& mesh : meshes)
		{
			// 硫붿떆蹂?蹂?蹂??(FX ??BoneIndex ?ㅼ뭡???泥?
			gbuf->PushModelBoneData(_model->GetBoneByIndex(mesh->boneIndex)->transform);

			if (mesh->material)
			{
				MaterialDesc& md = mesh->material->GetMaterialDesc();
				md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
				gbuf->PushMaterialData(md);

				auto diffuse = mesh->material->GetDiffuseMap();
				auto normal  = mesh->material->GetNormalMap();
				gbuf->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr);
				gbuf->SetPSSRV(2, normal  ? normal->GetComPtr().Get()  : nullptr);
			}
			RENDER_STATES->BindAllSamplersPS();

			mesh->vertexBuffer->PushData();
			mesh->indexBuffer->PushData();

			if (!ctx.buffer)
				gbuf->DrawIndexed(mesh->indexBuffer->GetCount(), 0, 0);
			else
			{
				ctx.buffer->PushData();
				gbuf->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
			}
		}
		return;
	}

	// ?? Shadow(depth-only) / SSAO(normal-depth) ?⑥뒪 (HLSL, ?뺤쟻 紐⑤뜽) ??
	if (ctx.shadowPass || ctx.ssaoPass)
	{
		auto shader = RESOURCES->Get<HlslShader>(ctx.shadowPass ? L"ShadowModel_HLSL" : L"SsaoNormalDepthModel_HLSL");
		if (!shader) return;

		shader->Bind();
		shader->PushGlobalData(ctx.view, ctx.proj);
		if (!ctx.buffer)
			shader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

		const auto& meshes = _model->GetMeshes();
		for (auto& mesh : meshes)
		{
			shader->PushModelBoneData(_model->GetBoneByIndex(mesh->boneIndex)->transform);

			if (mesh->material)
			{
				MaterialDesc& md = mesh->material->GetMaterialDesc();
				md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
				shader->PushMaterialData(md);
				auto diffuse = mesh->material->GetDiffuseMap();
				shader->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr); // ?뚰뙆?대┰??
			}
			RENDER_STATES->BindAllSamplersPS();

			mesh->vertexBuffer->PushData();
			mesh->indexBuffer->PushData();

			if (!ctx.buffer)
				shader->DrawIndexed(mesh->indexBuffer->GetCount(), 0, 0);
			else
			{
				ctx.buffer->PushData();
				shader->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
			}
		}
		return;
	}

	// ?? Preview/Thumbnail/Forward lit 寃쎈줈 (HLSL) ??
	// FX Standard/Thumbnail ?꾨━酉??뚮뜑瑜?HLSL 濡??泥?(FX ?곹깭 ?꾩닔濡??명븳 ???ㅼ뿼 ?댁냼)
	auto lit = RESOURCES->Get<HlslShader>(L"ModelPreview_HLSL");
	if (!lit) return;

	lit->Bind();
	lit->PushGlobalData(ctx.view, ctx.proj);
	if (!ctx.buffer)
		lit->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

	const auto& meshes = _model->GetMeshes();
	for (auto& mesh : meshes)
	{
		lit->PushModelBoneData(_model->GetBoneByIndex(mesh->boneIndex)->transform);

		if (mesh->material)
		{
			MaterialDesc& md = mesh->material->GetMaterialDesc();
			md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
			lit->PushMaterialData(md);
			auto diffuse = mesh->material->GetDiffuseMap();
			lit->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr);
		}
		RENDER_STATES->BindAllSamplersPS();

		mesh->vertexBuffer->PushData();
		mesh->indexBuffer->PushData();

		if (!ctx.buffer)
			lit->DrawIndexed(mesh->indexBuffer->GetCount(), 0, 0);
		else
		{
			ctx.buffer->PushData();
			lit->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
		}
	}
}

InstanceID ModelRenderer::GetInstanceID()
{
	return make_pair((uint64)_model.get(), (uint64)0);
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

	//占쌨시곤옙 占싹놂옙 占쏙옙 占쏙옙占?
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

