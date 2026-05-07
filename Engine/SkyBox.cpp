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
	GetGameObject()->SetEnableOutline(false);
	GetGameObject()->SetIgnoredTransformEdit(true);

	// HlslShader 기반 스카이박스
	HlslShaderDesc skyDesc;
	skyDesc.vsFile   = L"Sky.hlsl";
	skyDesc.psFile   = L"Sky.hlsl";
	skyDesc.vsEntry  = "VS_Main";
	skyDesc.psEntry  = "PS_Main";
	auto hlslShader = RESOURCES->GetOrAddHlslShader(L"Sky_HLSL", skyDesc);

	// RasterizerState: FrontCounterCW (안쪽 면 렌더링)
	hlslShader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::FrontCounterCW));
	// DepthStencilState: NoDepthWrite (스카이박스는 depth 쓰기 금지)
	hlslShader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));

	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(hlslShader);

	auto texture = RESOURCES->Load<Texture>(L"Sky", L"../Resources/Assets/Textures/Sky.jpg");
	material->SetDiffuseMap(texture);

	MaterialDesc& desc = material->GetMaterialDesc();
	desc.ambient  = Vec4(1.f);
	desc.diffuse  = Vec4(1.f);
	desc.specular = Vec4(1.f);
	RESOURCES->Add(L"Sky", material);

	if (GetGameObject()->GetMeshRenderer() == nullptr)
	{
		GetGameObject()->AddComponent(make_shared<MeshRenderer>());
		shared_ptr<MeshRenderer> renderer = GetGameObject()->GetMeshRenderer();
		auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
		if (renderer)
			renderer->SetMesh(mesh);
	}

	{
		auto mat = RESOURCES->Get<Material>(L"Sky");
		GetGameObject()->GetMeshRenderer()->SetMaterial(mat);
	}
}
