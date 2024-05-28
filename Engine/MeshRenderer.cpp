#include "pch.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Camera.h"
#include "Game.h"
#include "Shader.h"
#include "Light.h"
#include "MathUtils.h"
#include "Utils.h"

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
		shared_ptr<Shader> shader = _material->GetShader();
		std::string name = Utils::ToString(shader->GetName());

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


void MeshRenderer::Render(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	auto prevShader = _material->GetShader();

	if (shader)
		_material->SetShader(shader);

	auto curShader = _material->GetShader();
	// GlobalData
	curShader->PushGlobalData(V, P);

	auto world = GetTransform()->GetWorldMatrix();
	curShader->PushTransformData(TransformDesc{ world });

	//LIGHT 
	if (light)
		curShader->PushLightData(light->GetLightDesc());

	_material->Update();
	// IA
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	//DRAW 
	curShader->DrawIndexed(tech, _pass, _mesh->GetIndexBuffer()->GetCount(), 0, 0);

	//RESTORE 
	_material->SetShader(prevShader);
}

void MeshRenderer::RenderInstancing(int32 tech, shared_ptr<Shader> shader, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	auto prevShader = _material->GetShader();

	if(shader)
		_material->SetShader(shader);

	auto curShader = _material->GetShader();

	// GlobalData
	curShader->PushGlobalData(V, P);

	//LIGHT 
	if (light)
		curShader->PushLightData(light->GetLightDesc());

	_material->Update();
	// IA
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();
	buffer->PushData();

	//DRAW 
	curShader->DrawIndexedInstanced(tech, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
	
	//RESTORE 
	_material->SetShader(prevShader);
}

void MeshRenderer::RenderThumbnail(int32 tech, Matrix V, Matrix P, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer)
{
	auto shader = _material->GetShader();

	DCT->OMSetDepthStencilState(nullptr, 1);

	shader->PushGlobalData(V, P);
	shader->PushLightData(light->GetLightDesc());

	_material->Update();
	// IA
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	if (buffer == nullptr)
	{
		auto world = GetTransform()->GetWorldMatrix();
		shader->PushTransformData(TransformDesc{ world });
		shader->DrawIndexed(tech, _pass, _mesh->GetIndexBuffer()->GetCount(), 0, 0);
	}
	else
	{
		buffer->PushData();
		shader->DrawIndexedInstanced(tech, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
	}
}


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
