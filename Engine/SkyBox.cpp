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

	// HlslShader 스카이박스
	HlslShaderDesc skyDesc;
	skyDesc.vsFile   = L"Sky.hlsl";
	skyDesc.psFile   = L"Sky.hlsl";
	skyDesc.vsEntry  = "VS_Main";
	skyDesc.psEntry  = "PS_Main";
	auto hlslShader = RESOURCES->GetOrAddHlslShader(L"Sky_HLSL", skyDesc);

	hlslShader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::FrontCounterCW));
	hlslShader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth));

	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(hlslShader);
	material->SetRenderQueue(RenderQueue::Opaque);   // 항상 오파크 큐에 등록

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

	// 카메라 Far plane보다 살짝 작은 스피어로 생성
	GetGameObject()->GetOrAddTransform()->SetScale(Vec3(500.f, 500.f, 500.f));
}

void SkyBox::Update()
{
	// 항상 카메라 위치에 스카이박스를 배치 (Frustum Culling 회피)
	auto mainCamObj = SCENE->GetCurrentScene()->GetMainCamera();
	if (mainCamObj)
	{
		Vec3 camPos = mainCamObj->GetTransform()->GetPosition();
		GetTransform()->SetPosition(camPos);
	}
}
