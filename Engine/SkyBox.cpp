#include "pch.h"
#include "SkyBox.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "HlslShader.h"
#include "RenderStateManager.h"

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

	// HlslShader ?ㅼ뭅?대컯??
	HlslShaderDesc skyDesc;
	skyDesc.vsFile   = L"Sky.hlsl";
	skyDesc.psFile   = L"Sky.hlsl";
	skyDesc.vsEntry  = "VS_Main";
	skyDesc.psEntry  = "PS_Main";
	auto hlslShader = RESOURCES->GetOrAddHlslShader(L"Sky_HLSL", skyDesc);

	hlslShader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::FrontCounterCW));
	hlslShader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::SkyBoxDepth));

	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(hlslShader);
	material->SetRenderQueue(RenderQueue::Background); // 紐⑤뱺 遺덊닾紐?吏?ㅻ찓?몃━ ?댄썑 ?뚮뜑

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

	// 移대찓??Far plane蹂대떎 ?댁쭩 ?묒? ?ㅽ뵾?대줈 ?앹꽦
	GetGameObject()->GetOrAddTransform()->SetScale(Vec3(500.f, 500.f, 500.f));
}

void SkyBox::Update()
{
	// ??긽 移대찓???꾩튂???ㅼ뭅?대컯?ㅻ? 諛곗튂 (Frustum Culling ?뚰뵾)
	auto mainCamObj = SCENE->GetCurrentScene()->GetMainCamera();
	if (mainCamObj)
	{
		Vec3 camPos = mainCamObj->GetTransform()->GetPosition();
		GetTransform()->SetPosition(camPos);
	}
}
