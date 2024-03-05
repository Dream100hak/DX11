#include "pch.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "Game.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Light.h"
#include "MathUtils.h"

MeshRenderer::MeshRenderer() : Super(ComponentType::MeshRenderer)
{
	
}

MeshRenderer::~MeshRenderer()
{

}

void MeshRenderer::OnInspectorGUI()
{
	Super::OnInspectorGUI();

	if (_material != nullptr)
	{
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

void MeshRenderer::PreRenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	auto shader = RESOURCES->Get<Shader>(L"Shadow");

	if (shader == nullptr)
		return;

	// GlobalData
	shader->PushGlobalData(Light::S_MatView , Light::S_MatProjection);
	
	// Light
	auto lightObj = SCENE->GetCurrentScene()->GetLight();
	if (lightObj)
		shader->PushLightData(lightObj->GetLight()->GetLightDesc());

	_material->Update();
	// IA
	_mesh->GetVertexBuffer()->PushData();
	_mesh->GetIndexBuffer()->PushData();

	buffer->PushData();
	shader->DrawIndexedInstanced(0, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
}

void MeshRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
	if (_mesh == nullptr || _material == nullptr)
		return;

	auto shader = _material->GetShader();

	if (shader == nullptr)
		return;

	_material->SetShader(shader);

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	// GlobalData
	shader->PushGlobalData(cam->GetViewMatrix(), cam->GetProjectionMatrix());

	// Light
	auto lightObj = SCENE->GetCurrentScene()->GetLight();
	if (lightObj)
		shader->PushLightData(lightObj->GetLight()->GetLightDesc());
	

	{
		DCT->OMSetDepthStencilState(nullptr, 1);

		if (lightObj)
			shader->PushLightData(lightObj->GetLight()->GetLightDesc());

		_material->Update();
		// IA
		_mesh->GetVertexBuffer()->PushData();
		_mesh->GetIndexBuffer()->PushData();

		buffer->PushData();
		shader->DrawIndexedInstanced(_teq, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
	}

	{

		////Outline 
		if (GetGameObject()->GetEnableOutline())
		{
			DCT->OMSetDepthStencilState(GRAPHICS->GetDSStateOutline().Get(), 1);

			if (lightObj)
				shader->PushLightData(lightObj->GetLight()->GetLightDesc());

			_material->Update();
			// IA
			_mesh->GetVertexBuffer()->PushData();
			_mesh->GetIndexBuffer()->PushData();

			buffer->PushData();
			shader->DrawIndexedInstanced(1, _pass, _mesh->GetIndexBuffer()->GetCount(), buffer->GetCount());
		}
	}
	
}


void MeshRenderer::TransformBoundingBox()
{
	Matrix W = GetTransform()->GetWorldMatrix();

	Vec3 vMin = Vec3(MathUtils::INF, MathUtils::INF, MathUtils::INF);
	Vec3 vMax = Vec3(-MathUtils::INF, -MathUtils::INF, -MathUtils::INF);

	Vec3 corners[8];
	BoundingBox& meshBox = GetMesh()->GetMeshBox();

	meshBox.GetCorners(corners);

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

InstanceID MeshRenderer::GetInstanceID()
{
	return make_pair((uint64)_mesh.get(), (uint64)_material.get());
}

bool MeshRenderer::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
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