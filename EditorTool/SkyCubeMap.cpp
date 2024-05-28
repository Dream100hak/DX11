#include "pch.h"
#include "SkyCubeMap.h"
#include "MeshRenderer.h"
#include "Utils.h"

SkyCubeMap::SkyCubeMap()
{
	SetBehaviorName(Utils::ToWString(Utils::GetClassNameEX<SkyCubeMap>()));
}

SkyCubeMap::~SkyCubeMap()
{
}

void SkyCubeMap::Init(wstring fileName)
{
	_shader = make_shared<Shader>(L"01. CubeMap.fx");
	_cubeMapEffectBuffer = _shader->GetSRV("CubeMap");

	shared_ptr<Material> material = make_shared<Material>();
	material->SetShader(_shader);

	_cubeMap = RESOURCES->Load<Texture>(L"CubeMap", fileName);

	_cubeMapEffectBuffer->SetResource(_cubeMap->GetComPtr().Get());

	if (GetGameObject()->GetMeshRenderer() == nullptr)
	{
		GetGameObject()->AddComponent(make_shared<MeshRenderer>());
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
			GetGameObject()->GetMeshRenderer()->SetMesh(mesh);
		}
	}

	GetGameObject()->GetMeshRenderer()->SetMaterial(material);
}
