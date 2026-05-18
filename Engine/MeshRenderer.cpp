#include "pch.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Camera.h"
#include "Game.h"
#include "Shader.h"
#include "HlslShader.h"
#include "Light.h"
#include "MathUtils.h"
#include "Utils.h"
#include "RenderContext.h"
#include "BindShaderDesc.h"  // ? LightArrayDesc 필요

MeshRenderer::MeshRenderer() : Super(RendererType::Mesh)
{
}


MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::OnInspectorGUI()
{
	if (_material != nullptr)
	{
		// FX11 Shader 또는 HlslShader 이름 표시 (nullptr 방어)
		std::string name = "(no shader)";
		if (auto shader = _material->GetShader())
			name = Utils::ToString(shader->GetName());
		else if (auto hlsl = _material->GetHlslShader())
			name = Utils::ToString(hlsl->GetName());

		ImGui::Text(name.c_str());

		MaterialDesc& desc = _material->GetMaterialDesc();
		ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

		if (ImGui::ColorEdit3("Diffuse", (float*)&desc.diffuse)) {}
		if (ImGui::ColorEdit3("Ambient", (float*)&desc.ambient)) {}
		if (ImGui::ColorEdit3("Emissive", (float*)&desc.emissive)) {}
		if (ImGui::ColorEdit3("Specular", (float*)&desc.specular)) {}

		ImGui::NewLine();

		// Diffuse Map
		{
			ImGui::BeginGroup();
			ImGui::TextColored(color, "Diffuse");

			if (_material->GetDiffuseMap() != nullptr)
				ImGui::Image((void*)_material->GetDiffuseMap()->GetComPtr().Get(), ImVec2(75, 75));
			else
				ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));

			ImGui::EndGroup();
		}


		ImGui::SameLine(0.f, -2.f); // 같은 줄에 배치

		// Normal Map
		{
			ImGui::BeginGroup();
			ImGui::TextColored(color, "Normal");

			if (_material->GetNormalMap() != nullptr)
				ImGui::Image((void*)_material->GetNormalMap()->GetComPtr().Get(), ImVec2(75, 75));
			else
				ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));

			ImGui::EndGroup();
		}


		ImGui::SameLine(); // 같은 줄에 배치

		// Specular Map
		{
			ImGui::BeginGroup();
			ImGui::TextColored(color, "Specular");

			if (_material->GetSpecularMap() != nullptr)
				ImGui::Image((void*)_material->GetSpecularMap()->GetComPtr().Get(), ImVec2(75, 75));

			else
				ImGui::Image((void*)RESOURCES->Get<Texture>(L"Grid")->GetComPtr().Get(), ImVec2(75, 75));

			ImGui::EndGroup();
		}
	}
}

// ============================================================
// Draw() ? 렌더 진입 지점
//  ctx.buffer == nullptr  ? 단일 드로우
//  ctx.buffer != nullptr  ? 인스턴싱 드로우
//  ctx.shaderOverride     ? 셰이더 오버라이드 (Shadow/Outline 패스)
//  ctx.hlslOverride       ? HlslShader 기반 오버라이드
// ============================================================
void MeshRenderer::Draw(const RenderContext& ctx)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	// ── HlslShader override 우선 처리 ─────────────────────────────────────────────
	if (ctx.hlslOverride)
	{
		auto hlsl = ctx.hlslOverride;
		hlsl->Bind();
		hlsl->PushGlobalData(ctx.view, ctx.proj);
		if (ctx.light) hlsl->PushLightData(ctx.light->GetLightDesc());

		if (!ctx.buffer) // Single
			hlsl->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

		_material->Update();
		_mesh->GetVertexBuffer()->PushData();
		_mesh->GetIndexBuffer()->PushData();

		if (!ctx.buffer)
			hlsl->DrawIndexed(_mesh->GetIndexBuffer()->GetCount(), 0, 0);
		else
		{
			ctx.buffer->PushData();
			hlsl->DrawIndexedInstanced(_mesh->GetIndexBuffer()->GetCount(), ctx.buffer->GetCount());
		}
		return;
	}

	// ── HlslShader 경로 ────────────────────────────────────────────────────────────
	if (auto hlsl = _material->GetHlslShader())
	{
		hlsl->Bind();
		hlsl->PushGlobalData(ctx.view, ctx.proj);
		
		// ? 라이트 배열 우선, 없으면 단일 라이트
		if (ctx.lightArray)
			hlsl->PushLightArrayData(*ctx.lightArray);
		else if (ctx.light)
			hlsl->PushLightData(ctx.light->GetLightDesc());

		if (!ctx.buffer) // Single
			hlsl->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

		_material->Update();
		_mesh->GetVertexBuffer()->PushData();
		_mesh->GetIndexBuffer()->PushData();

		if (!ctx.buffer)
			hlsl->DrawIndexed(_mesh->GetIndexBuffer()->GetCount(), 0, 0);
		else
		{
			ctx.buffer->PushData();
			hlsl->DrawIndexedInstanced(_mesh->GetIndexBuffer()->GetCount(), ctx.buffer->GetCount());
		}
		return;
	}

	// ── FX11 경로 ─────────────────────────────────────────────────────────────────
	auto prevShader = _material->GetShader();
	if (ctx.shaderOverride) _material->SetShader(ctx.shaderOverride);

	auto curShader = _material->GetShader();
	if (curShader == nullptr) return;

	curShader->PushGlobalData(ctx.view, ctx.proj);
	if (ctx.light) curShader->PushLightData(ctx.light->GetLightDesc());

	if (!ctx.buffer)
		curShader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

	_material->Update();
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	if (!ctx.buffer)
		curShader->DrawIndexed(ctx.tech, _pass, _mesh->GetIndexBuffer()->GetCount(), 0, 0);
	else
	{
		ctx.buffer->PushData();
		curShader->DrawIndexedInstanced(ctx.tech, _pass, _mesh->GetIndexBuffer()->GetCount(), ctx.buffer->GetCount());
	}

	_material->SetShader(prevShader);
}

// ============================================================
// Pick() - 마우스 선택 로직
// ============================================================
bool MeshRenderer::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
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

	TransformBoundingBox();
	float dist = 0.f;

	if (_boundingBox.Intersects(ray.position, ray.direction, dist))
	{
		const auto& vertices = _mesh->GetGeometry()->GetVertices();
		const auto& indices = _mesh->GetGeometry()->GetIndices();

		for (uint32 i = 0; i < indices.size() / 3; ++i)
		{
			uint32 i0 = indices[i * 3 + 0];
			uint32 i1 = indices[i * 3 + 1];
			uint32 i2 = indices[i * 3 + 2];

			Vec3 v0 = XMVector3TransformCoord(vertices[i0].position, W);
			Vec3 v1 = XMVector3TransformCoord(vertices[i1].position, W);
			Vec3 v2 = XMVector3TransformCoord(vertices[i2].position, W);

			if (ray.Intersects(v0, v1, v2, OUT distance))
			{
				pickPos = ray.position + ray.direction * distance;
				return true;
			}
		}
	}

	return false;

}


InstanceID MeshRenderer::GetInstanceID()
{
	return make_pair((uint64)_mesh.get(), (uint64)_material.get());
}
