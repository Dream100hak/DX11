#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
#include "HlslShader.h"
#include "RenderStateManager.h"
#include <filesystem>

void ResourceManager::Init()
{
	CreateDefaultMesh();
	CreateDefaultShader();
	CreateDefaultMaterial();

	CreateShadowMapShader();
	CreateOutlineShader();
	CreateThumbnailShader();
	CreateSSAOShader();
	CreateTerrainShader();
	CreateDeferredShaders();
	CreateEditorMiscShaders();
	CreateParticleShaders();
}

void ResourceManager::CreateParticleShaders()
{
	// 파티클 스트림 아웃 정점 구조: POS.xyz / VELOCITY.xyz / SIZE.xy / AGE.x / TYPE.x (40 bytes)
	// FX의 ConstructGSWithSO("POS.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; TYPE.x") 대체
	const vector<D3D11_SO_DECLARATION_ENTRY> soEntries =
	{
		{ 0, "POS",      0, 0, 3, 0 },
		{ 0, "VELOCITY", 0, 0, 3, 0 },
		{ 0, "SIZE",     0, 0, 2, 0 },
		{ 0, "AGE",      0, 0, 1, 0 },
		{ 0, "TYPE",     0, 0, 1, 0 },
	};
	const uint32 soStride = sizeof(float) * 9 + sizeof(uint32); // 40

	// 불 파티클 셰이더
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Fire.hlsl";
		desc.gsFile  = L"Fire.hlsl";
		desc.vsEntry = "VS_StreamOut";
		desc.gsEntry = "GS_StreamOut";
		desc.soEntries = soEntries;
		desc.soStride  = soStride;
		auto s = GetOrAddHlslShader(L"FireSO_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth));
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Fire.hlsl";
		desc.gsFile  = L"Fire.hlsl";
		desc.psFile  = L"Fire.hlsl";
		desc.vsEntry = "VS_Draw";
		desc.gsEntry = "GS_Draw";
		desc.psEntry = "PS_Draw";
		auto s = GetOrAddHlslShader(L"FireDraw_HLSL", desc);
		if (s)
		{
			s->SetBlendState(RENDER_STATES->GetBS(BlendStateType::AdditiveSrcAlpha));
			s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));
		}
	}

	// 빗 파티클 셰이더
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Rain.hlsl";
		desc.gsFile  = L"Rain.hlsl";
		desc.vsEntry = "VS_StreamOut";
		desc.gsEntry = "GS_StreamOut";
		desc.soEntries = soEntries;
		desc.soStride  = soStride;
		auto s = GetOrAddHlslShader(L"RainSO_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::DisableDepth));
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Rain.hlsl";
		desc.gsFile  = L"Rain.hlsl";
		desc.psFile  = L"Rain.hlsl";
		desc.vsEntry = "VS_Draw";
		desc.gsEntry = "GS_Draw";
		desc.psEntry = "PS_Draw";
		auto s = GetOrAddHlslShader(L"RainDraw_HLSL", desc);
		if (s) s->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::NoDepthWrite));
	}
}

std::shared_ptr<Texture> ResourceManager::GetOrAddTexture(const wstring& key, const wstring& path)
{
	shared_ptr<Texture> texture = Get<Texture>(key);

	if (filesystem::exists(filesystem::path(path)) == false)
		return nullptr;

	texture = Load<Texture>(key, path);

	if (texture == nullptr)
	{
		texture = make_shared<Texture>();
		texture->Load(path);
		Add(key, texture);
	}

	return texture;
}

shared_ptr<HlslShader> ResourceManager::GetOrAddHlslShader(const wstring& key, const HlslShaderDesc& desc)
{
	// 셰이더 버킷에서 검색 (ResourceType::Shader 기준)
	auto& bucket = _resources[static_cast<uint8>(ResourceType::Shader)];
	auto it = bucket.find(key);
	if (it != bucket.end())
		return static_pointer_cast<HlslShader>(it->second);

	auto shader = make_shared<HlslShader>();
	shader->SetName(key);
	shader->Create(desc);
	bucket[key] = shader;
	return shader;
}

void ResourceManager::CreateDefaultMesh()
{
	// 이름은 씬 직렬화의 프리미티브 식별자로도 사용
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateQuad();
		mesh->SetName(L"Quad");
		Add(L"Quad", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateCube();
		mesh->SetName(L"Cube");
		Add(L"Cube", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateSphere();
		mesh->SetName(L"Sphere");
		Add(L"Sphere", mesh);
	}
}

void ResourceManager::CreateDefaultShader()
{
	// HlslShader 표준 셰이더 (FX 01. Standard.fx 대체)
	HlslShaderDesc hlslDesc;
	hlslDesc.vsFile  = L"Standard_VS.hlsl";
	hlslDesc.psFile  = L"Standard_PS.hlsl";
	hlslDesc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl의 메시 버텍스 셰이더 진입점
	hlslDesc.psEntry = "PS_Main";
	GetOrAddHlslShader(L"Standard_HLSL", hlslDesc);
}

void ResourceManager::CreateDefaultMaterial()
{
	// HlslShader를 사용하는 기본 재질 생성
	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(Get<HlslShader>(L"Standard_HLSL"));
	MaterialDesc& desc = material->GetMaterialDesc();
	RESOURCES->Add(L"DefaultMaterial", material);
}

void ResourceManager::CreateShadowMapShader()
{
	// 깊이 전용 셰이더 (HLSL). Standard_VS를 재사용하며 변환은 라이트의 VP로 수행하고
	// 라이트 VP는 PushGlobalData(lightV, lightP)로 b0 VP에 설정된다. PS는 알파 클립만 수행.
	auto shadowRS = RENDER_STATES->GetRS(RasterizerStateType::ShadowDepth);
	// 메시 셰이더
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"Shadow_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
	// 정적 모델 셰이더
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"ShadowModel_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
	// 애니메이션 모델 셰이더
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"ShadowMap_PS.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_AlphaClip";
		if (auto s = GetOrAddHlslShader(L"ShadowAnim_HLSL", desc)) s->SetRasterizerState(shadowRS);
	}
}

void ResourceManager::CreateOutlineShader()
{
	// 선택 아웃라인 (스텐실 2패스, Camera::RenderOutlinePass) — 렌더러 타입별 VS 진입점
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Outline_VS.hlsl";
		desc.psFile  = L"Outline_PS.hlsl";
		desc.vsEntry = "VS_MeshOutline";
		GetOrAddHlslShader(L"Outline_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Outline_VS.hlsl";
		desc.psFile  = L"Outline_PS.hlsl";
		desc.vsEntry = "VS_ModelOutline";
		GetOrAddHlslShader(L"OutlineModel_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Outline_VS.hlsl";
		desc.psFile  = L"Outline_PS.hlsl";
		desc.vsEntry = "VS_AnimationOutline";
		GetOrAddHlslShader(L"OutlineAnim_HLSL", desc);
	}
}

void ResourceManager::CreateThumbnailShader()
{
	// HLSL (FX 01. Thumbnail.fx 대체). 썸네일을 그리는 ModelPreview_HLSL/AnimPreview_HLSL도 있음
	HlslShaderDesc desc;
	desc.vsFile  = L"Standard_VS.hlsl";
	desc.psFile  = L"Thumbnail.hlsl";
	desc.vsEntry = "VS_Mesh";   // Standard_VS.hlsl의 메시 버텍스 셰이더 진입점
	desc.psEntry = "PS_Solid";
	GetOrAddHlslShader(L"Thumbnail_HLSL", desc);

	// 머티리얼 프리뷰 구체 (정적 메시) — 모델 썸네일과 동일한 PS_PreviewLit
	// (씬용 Standard_PS 는 섀도우맵/라이트배열 의존이라 썸네일 패스에서 검게 나옴)
	{
		HlslShaderDesc meshPrev;
		meshPrev.vsFile  = L"Standard_VS.hlsl";
		meshPrev.psFile  = L"Thumbnail.hlsl";
		meshPrev.vsEntry = "VS_Mesh";
		meshPrev.psEntry = "PS_PreviewLit";
		GetOrAddHlslShader(L"MeshPreview_HLSL", meshPrev);
	}
}

void ResourceManager::CreateSSAOShader()
{
	// SSAO 노말-깊이 셰이더 (모델 렌더링) 및 HLSL. FX 00. SsaoNormalDepth.fx는 모델 그리기를 수행했음
	// 뷰 공간 노말 + 뷰 공간 깊이를 출력함.
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepth_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepthModel_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoNormalDepth.hlsl";
		desc.psFile  = L"SsaoNormalDepth.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"SsaoNormalDepthAnim_HLSL", desc);
	}

	// SSAO 연산 (HLSL) 및 FX 00. Ssao.fx 대체. 텍스처를 바인딩하는 Ssao 클래스 내에서 직접 바인딩함
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Ssao.hlsl";
		desc.psFile  = L"Ssao.hlsl";
		GetOrAddHlslShader(L"Ssao_HLSL", desc);
	}
	// SSAO 블러 (HLSL) 및 FX 00. SsaoBlur.fx 대체. 가로/세로는 cbuffer HorzBlur로 구분
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"SsaoBlur.hlsl";
		desc.psFile  = L"SsaoBlur.hlsl";
		GetOrAddHlslShader(L"SsaoBlur_HLSL", desc);
	}
}

void ResourceManager::CreateDeferredShaders()
{
	// G-Buffer fill (VS_Mesh + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Mesh";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBuffer_HLSL", desc);
	}

	// G-Buffer fill for static models (VS_Model + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBufferModel_HLSL", desc);
	}

	// G-Buffer fill for animated models (VS_Animation + GBuffer PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"GBuffer_PS.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"GBufferAnim_HLSL", desc);
	}

	// 식생(잔디) GBuffer — 인스턴스 쿼드 + 바람 + 절차적 블레이드 알파컷
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Grass.hlsl";
		desc.psFile  = L"Grass.hlsl";
		desc.vsEntry = "VS_Grass";
		desc.psEntry = "PS_Grass";
		GetOrAddHlslShader(L"Grass_GBuffer_HLSL", desc);
	}

	// 식생(나무) GBuffer — 인스턴스 저폴리 메시(줄기+캐노피)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Tree.hlsl";
		desc.psFile  = L"Tree.hlsl";
		desc.vsEntry = "VS_Tree";
		desc.psEntry = "PS_Tree";
		GetOrAddHlslShader(L"Tree_GBuffer_HLSL", desc);
	}

	// Deferred Lighting (fullscreen triangle, no vertex buffer)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"DeferredLighting.hlsl";
		desc.psFile  = L"DeferredLighting.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"DeferredLighting_HLSL", desc);
	}

	// SSR (스크린스페이스 반사, 풀스크린)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Ssr.hlsl";
		desc.psFile  = L"Ssr.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Ssr_HLSL", desc);
	}

	// Auto-exposure 로그휘도 (풀스크린 → 밉체인)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Exposure.hlsl";
		desc.psFile  = L"Exposure.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Luminance";
		GetOrAddHlslShader(L"Luminance_HLSL", desc);
	}

	// 볼류메트릭 라이트 (갓레이, 풀스크린)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Volumetric.hlsl";
		desc.psFile  = L"Volumetric.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Volumetric_HLSL", desc);
	}

	// Depth of Field (풀스크린)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Dof.hlsl";
		desc.psFile  = L"Dof.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Dof_HLSL", desc);
	}

	// Tonemap (HDR sceneColor -> 백스크린 ACES + 감마 보정)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Tonemap.hlsl";
		desc.psFile  = L"Tonemap.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Tonemap_HLSL", desc);
	}

	// IBL 베이킹 (초기화는 Ibl::Init 참조)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"IblBake.hlsl";
		desc.psFile  = L"IblBake.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Irradiance";
		GetOrAddHlslShader(L"IblIrradiance_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"IblBake.hlsl";
		desc.psFile  = L"IblBake.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Prefilter";
		GetOrAddHlslShader(L"IblPrefilter_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"IblBake.hlsl";
		desc.psFile  = L"IblBake.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_BrdfLut";
		GetOrAddHlslShader(L"IblBrdf_HLSL", desc);
	}

	// Bloom: BrightPass / BlurH / BlurV (핑퐁 텍스처)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"PostProcess.hlsl";
		desc.psFile  = L"PostProcess.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_BrightPass";
		GetOrAddHlslShader(L"BloomBright_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"PostProcess.hlsl";
		desc.psFile  = L"PostProcess.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_BlurH";
		GetOrAddHlslShader(L"BloomBlurH_HLSL", desc);
	}
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"PostProcess.hlsl";
		desc.psFile  = L"PostProcess.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_BlurV";
		GetOrAddHlslShader(L"BloomBlurV_HLSL", desc);
	}

	// FXAA (풀스크린 LDR 안티앨리어싱)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Fxaa.hlsl";
		desc.psFile  = L"Fxaa.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Fxaa_HLSL", desc);
	}

	// Pass Viewer (중간 렌더 결과 시각화: Albedo/Normal/Roughness/Metallic/Depth/SSAO/Shadow)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"PassViewer.hlsl";
		desc.psFile  = L"PassViewer.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"PassViewer_HLSL", desc);
	}

	// G-Buffer Debug View (4-분할)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"GBufferDebug.hlsl";
		desc.psFile  = L"GBufferDebug.hlsl";
		desc.vsEntry = "VS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"GBufferDebug_HLSL", desc);
	}
}

void ResourceManager::CreateEditorMiscShaders()
{
	// 씬 그리드 (실선 렌더 및 좌표축 그리드) 및 FX11 01. SceneGrid.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"SceneGrid.hlsl";
		desc.psFile = L"SceneGrid.hlsl";
		GetOrAddHlslShader(L"SceneGrid_HLSL", desc);
	}

	// 콜라이더 (씬 바운드박스, 정점 컬러) 및 FX11 01. Collider.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"Collider.hlsl";
		desc.psFile = L"Collider.hlsl";
		auto shader = GetOrAddHlslShader(L"Collider_HLSL", desc);
		if (shader)
		{
			shader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::Default));
			shader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::SolidCullNone));
		}
	}

	// 큐브맵 스카이박스 (SkyCubeMap, 환경맵) 및 FX11 01. CubeMap.fx 대체
	{
		HlslShaderDesc desc;
		desc.vsFile = L"CubeMap.hlsl";
		desc.psFile = L"CubeMap.hlsl";
		auto shader = GetOrAddHlslShader(L"CubeMap_HLSL", desc);
		if (shader)
		{
			shader->SetDepthStencilState(RENDER_STATES->GetDSS(DepthStencilStateType::SkyBoxDepth));
			shader->SetRasterizerState(RENDER_STATES->GetRS(RasterizerStateType::FrontCounterCW));
		}
	}

	// 모델 썸네일 렌더링 (정적 모델) 는 VS_Model + PS_PreviewLit. FX Standard/Thumbnail 대체
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"Thumbnail.hlsl";
		desc.vsEntry = "VS_Model";
		desc.psEntry = "PS_PreviewLit";
		GetOrAddHlslShader(L"ModelPreview_HLSL", desc);
	}

	// 모델 썸네일 렌더링 (애니메이션 모델) 는 VS_Animation + PS_PreviewLit
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Standard_VS.hlsl";
		desc.psFile  = L"Thumbnail.hlsl";
		desc.vsEntry = "VS_Animation";
		desc.psEntry = "PS_PreviewLit";
		GetOrAddHlslShader(L"AnimPreview_HLSL", desc);
	}
}

void ResourceManager::CreateTerrainShader()
{
	// Terrain HLSL (VS + HS + DS + PS)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_Main";
		GetOrAddHlslShader(L"Terrain_HLSL", desc);
	}

	// Terrain Shadow HLSL (VS + HS + DS only, depth-only pass)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		GetOrAddHlslShader(L"Terrain_Shadow_HLSL", desc);
	}

	// Terrain GBuffer HLSL (VS + HS + DS + PS_GBuffer) 지형용 GBuffer fill
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_GBuffer";
		GetOrAddHlslShader(L"Terrain_GBuffer_HLSL", desc);
	}

	// Terrain SSAO normal-depth HLSL (VS + HS + DS + PS_NormalDepth)
	// SSAO 셰이더에서 지형이 뷰 공간 노말+깊이를 기여하도록 함 (깊이는 높이맵 그래디언트 계산)
	{
		HlslShaderDesc desc;
		desc.vsFile  = L"Terrain.hlsl";
		desc.hsFile  = L"Terrain.hlsl";
		desc.dsFile  = L"Terrain.hlsl";
		desc.psFile  = L"Terrain.hlsl";
		desc.vsEntry = "VS_Main";
		desc.hsEntry = "HS_Main";
		desc.dsEntry = "DS_Main";
		desc.psEntry = "PS_NormalDepth";
		GetOrAddHlslShader(L"Terrain_NormalDepth_HLSL", desc);
	}
}
