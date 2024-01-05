#include "pch.h"
#include "SkyBox.h"
#include "Material.h"
#include "MeshRenderer.h"


SkyBox::SkyBox() : Super(ComponentType::SkyBox)
{

}

SkyBox::~SkyBox()
{
	
}

void SkyBox::Init(SkyType type)
{
	GetGameObject()->SetEnableOutline(false);
	GetGameObject()->SetIgnoredTransformEdit(true);

	if (type == SkyType::SkyBox)
	{
		auto shader = make_shared<Shader>(L"01. Sky.fx");

		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(shader);
		auto texture = RESOURCES->Load<Texture>(L"Sky", L"../Resources/Textures/Sky01.jpg");

		material->SetDiffuseMap(texture);
		MaterialDesc& desc = material->GetMaterialDesc();

		desc.ambient = Vec4(1.f);
		desc.diffuse = Vec4(1.f);
		desc.specular = Vec4(1.f);
		RESOURCES->Add(L"Sky", material);

		if (GetGameObject()->GetMeshRenderer() == nullptr)
		{
			GetGameObject()->AddComponent(make_shared<MeshRenderer>());
			{
				auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
				GetGameObject()->GetMeshRenderer()->SetMesh(mesh);
			}
		}

		{
			auto material = RESOURCES->Get<Material>(L"Sky");
			GetGameObject()->GetMeshRenderer()->SetMaterial(material);
		}
	}
	else if(type == SkyType::CubeMap)
	{
		auto shader = make_shared<Shader>(L"01. CubeMap.fx");

		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(shader);
		auto texture = RESOURCES->Load<Texture>(L"Sky", L"../Resources/Assets/Textures/desertcube1024.dds");

		material->SetCubeMap(texture);
		MaterialDesc& desc = material->GetMaterialDesc();

		desc.ambient = Vec4(1.f);
		desc.diffuse = Vec4(1.f);
		desc.specular = Vec4(1.f);
		RESOURCES->Add(L"Sky", material);

		if (GetGameObject()->GetMeshRenderer() == nullptr)
		{
			GetGameObject()->AddComponent(make_shared<MeshRenderer>());
			{
				auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
				GetGameObject()->GetMeshRenderer()->SetMesh(mesh);
			}
		}

		{
			auto material = RESOURCES->Get<Material>(L"Sky");
			GetGameObject()->GetMeshRenderer()->SetMaterial(material);
		}
	}

}
