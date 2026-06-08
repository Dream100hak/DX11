#include "pch.h"
#include "SkyCubeMap.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "HlslShader.h"
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
	_cubeMap = RESOURCES->Load<Texture>(L"CubeMap", fileName);

	// FX11 01. CubeMap.fx → HLSL CubeMap.hlsl. 큐브 텍스처는 머티리얼 DiffuseMap(t0)으로 전달.
	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(RESOURCES->Get<HlslShader>(L"CubeMap_HLSL"));
	material->SetDiffuseMap(_cubeMap);

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
