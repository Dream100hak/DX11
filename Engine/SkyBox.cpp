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

void SkyBox::Init()
{
	auto shader = make_shared<Shader>(L"18. SkyDemo.fx");

	shared_ptr<Material> material = make_shared<Material>();
	material->SetShader(shader);
	auto texture = RESOURCES->Load<Texture>(L"Sky", L"..\\Resources\\Textures\\sky01.jpg");

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
