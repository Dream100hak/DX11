#include "pch.h"
#include "SceneSerializer.h"
#include "tinyxml2.h"
#include <filesystem>

#include "GameObject.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Model.h"
#include "ModelAnimation.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"
#include "Light.h"
#include "Terrain.h"
#include "SkyCubeMap.h"
#include "ParticleSystem.h"

#include "EditorToolManager.h"
#include "LogWindow.h"
#include "Utils.h"

using namespace tinyxml2;

namespace
{
	// ── 공통 어트리뷰트 헬퍼 ──────────────────────────────────────────────

	void WriteVec3(XMLElement* parent, const char* name, const Vec3& v)
	{
		XMLElement* el = parent->GetDocument()->NewElement(name);
		el->SetAttribute("x", v.x);
		el->SetAttribute("y", v.y);
		el->SetAttribute("z", v.z);
		parent->LinkEndChild(el);
	}

	void WriteColor(XMLElement* parent, const char* name, const Color& c)
	{
		XMLElement* el = parent->GetDocument()->NewElement(name);
		el->SetAttribute("r", c.x);
		el->SetAttribute("g", c.y);
		el->SetAttribute("b", c.z);
		el->SetAttribute("a", c.w);
		parent->LinkEndChild(el);
	}

	void WriteWstrAttr(XMLElement* el, const char* name, const wstring& value)
	{
		el->SetAttribute(name, Utils::ToString(value).c_str());
	}

	// ── 머티리얼: .mat 파일 기반이면 참조, 아니면 인라인 ──────────────────

	void WriteMaterial(XMLElement* parent, shared_ptr<Material> material)
	{
		if (material == nullptr)
			return;

		tinyxml2::XMLDocument* doc = parent->GetDocument();

		// Material::Load 가 SetName(<풀경로>.mat) 하므로 이름 끝이 .mat 이면 파일 참조
		const wstring& matName = material->GetName();
		bool fileBacked = matName.size() >= 4 &&
			_wcsicmp(matName.substr(matName.size() - 4).c_str(), L".mat") == 0;

		if (fileBacked)
		{
			XMLElement* refEl = doc->NewElement("MaterialRef");
			WriteWstrAttr(refEl, "path", matName);
			parent->LinkEndChild(refEl);
			return;
		}

		// 클론 머티리얼 — MaterialDesc + 텍스처 경로 인라인
		MaterialDesc& desc = material->GetMaterialDesc();

		XMLElement* matEl = doc->NewElement("Material");
		matEl->SetAttribute("useTexture", desc.useTexture);
		matEl->SetAttribute("useAlphaclip", desc.useAlphaclip);
		matEl->SetAttribute("useSsao", desc.useSsao);
		matEl->SetAttribute("lightCount", desc.lightCount);
		matEl->SetAttribute("roughness", desc.roughness);
		matEl->SetAttribute("metallic", desc.metallic);
		matEl->SetAttribute("renderQueue", (int)material->GetRenderQueue());

		WriteColor(matEl, "Ambient", desc.ambient);
		WriteColor(matEl, "Diffuse", desc.diffuse);
		WriteColor(matEl, "Specular", desc.specular);
		WriteColor(matEl, "Emissive", desc.emissive);

		XMLElement* maps = doc->NewElement("Maps");
		WriteWstrAttr(maps, "diffuse", material->GetDiffuseMap() ? material->GetDiffuseMap()->GetPath() : L"");
		WriteWstrAttr(maps, "normal", material->GetNormalMap() ? material->GetNormalMap()->GetPath() : L"");
		WriteWstrAttr(maps, "specular", material->GetSpecularMap() ? material->GetSpecularMap()->GetPath() : L"");
		matEl->LinkEndChild(maps);

		parent->LinkEndChild(matEl);
	}

	// ── 게임오브젝트 1개 직렬화. 미지원 컴포넌트는 경고 로그 ──────────────

	void WriteGameObject(XMLElement* root, shared_ptr<GameObject> obj)
	{
		tinyxml2::XMLDocument* doc = root->GetDocument();

		XMLElement* objEl = doc->NewElement("GameObject");
		WriteWstrAttr(objEl, "name", obj->GetObjectName());
		objEl->SetAttribute("layer", obj->GetLayerIndex());

		// Transform (부모 없는 평면 구조 — 로컬 = 월드)
		if (auto tr = obj->GetTransform())
		{
			XMLElement* trEl = doc->NewElement("Transform");
			WriteVec3(trEl, "Position", tr->GetLocalPosition());
			WriteVec3(trEl, "Rotation", tr->GetLocalRotation());
			WriteVec3(trEl, "Scale", tr->GetLocalScale());
			objEl->LinkEndChild(trEl);
		}

		// 렌더러 — 구체 타입별 분기
		if (auto renderer = obj->GetRenderer())
		{
			switch (renderer->GetRenderType())
			{
			case RendererType::Mesh:
			{
				auto mr = static_pointer_cast<MeshRenderer>(renderer);
				XMLElement* el = doc->NewElement("MeshRenderer");
				WriteWstrAttr(el, "mesh", mr->GetMesh() ? mr->GetMesh()->GetName() : L"");
				el->SetAttribute("technique", mr->GetTechnique());
				WriteMaterial(el, mr->GetMaterial());
				objEl->LinkEndChild(el);
				break;
			}
			case RendererType::Model:
			{
				auto mr = static_pointer_cast<ModelRenderer>(renderer);
				XMLElement* el = doc->NewElement("ModelRenderer");
				WriteWstrAttr(el, "model", mr->GetModel() ? mr->GetModel()->GetRelativePath() : L"");
				el->SetAttribute("technique", mr->GetTechnique());
				objEl->LinkEndChild(el);
				break;
			}
			case RendererType::Animator:
			{
				auto ma = static_pointer_cast<ModelAnimator>(renderer);
				XMLElement* el = doc->NewElement("ModelAnimator");
				WriteWstrAttr(el, "model", ma->GetModel() ? ma->GetModel()->GetRelativePath() : L"");
				el->SetAttribute("technique", ma->GetTechnique());
				el->SetAttribute("animIndex", ma->GetTweenDesc().curr.animIndex);

				if (ma->GetModel())
				{
					for (auto& anim : ma->GetModel()->GetAnimations())
					{
						XMLElement* clipEl = doc->NewElement("Clip");
						WriteWstrAttr(clipEl, "file", anim->fileName);
						el->LinkEndChild(clipEl);
					}
				}
				objEl->LinkEndChild(el);
				break;
			}
			case RendererType::Particle:
			{
				auto ps = static_pointer_cast<ParticleSystem>(renderer);
				XMLElement* el = doc->NewElement("ParticleSystem");
				el->SetAttribute("type", ps->GetType());
				el->SetAttribute("max", ps->GetMaxParticles());
				el->SetAttribute("emitInterval", ps->GetEmitInterval());
				el->SetAttribute("lifetime", ps->GetLifetime());
				el->SetAttribute("initialSpeed", ps->GetInitialSpeed());
				el->SetAttribute("sizeX", ps->GetParticleSize().x);
				el->SetAttribute("sizeY", ps->GetParticleSize().y);
				WriteVec3(el, "Accel", ps->GetAccel());
				WriteVec3(el, "EmitDir", ps->GetEmitDir());

				for (const wstring& tex : ps->GetTextureNames())
				{
					XMLElement* texEl = doc->NewElement("Texture");
					WriteWstrAttr(texEl, "file", tex);
					el->LinkEndChild(texEl);
				}
				objEl->LinkEndChild(el);
				break;
			}
			default:
				ADDLOG("Save Scene : unsupported renderer skipped (" +
					Utils::ToString(obj->GetObjectName()) + ")", LogFilter::Warn);
				break;
			}
		}

		// Light
		if (auto light = obj->GetLight())
		{
			LightDesc& desc = light->GetLightDesc();

			XMLElement* el = doc->NewElement("Light");
			el->SetAttribute("type", (int)light->GetLightType());
			el->SetAttribute("intensity", light->GetIntensity());
			el->SetAttribute("range", light->GetRange());
			el->SetAttribute("spotAngle", light->GetSpotAngleDeg());
			WriteColor(el, "Ambient", desc.ambient);
			WriteColor(el, "Diffuse", desc.diffuse);
			WriteColor(el, "Specular", desc.specular);
			WriteVec3(el, "Direction", desc.direction);
			WriteVec3(el, "Attenuation", light->GetAttenuation());
			objEl->LinkEndChild(el);
		}

		// Terrain
		if (auto terrain = obj->GetTerrain())
		{
			const TerrainInfo& info = terrain->GetInfo();

			XMLElement* el = doc->NewElement("Terrain");
			WriteWstrAttr(el, "heightMap", info.heightMapFilename);
			WriteWstrAttr(el, "blendMap", info.blendMapFilename);
			el->SetAttribute("heightScale", info.heightScale);
			el->SetAttribute("width", info.heightmapWidth);
			el->SetAttribute("height", info.heightmapHeight);
			el->SetAttribute("cellSpacing", info.cellSpacing);

			for (const wstring& layer : info.layerMapFilenames)
			{
				XMLElement* layerEl = doc->NewElement("Layer");
				WriteWstrAttr(layerEl, "file", layer);
				el->LinkEndChild(layerEl);
			}
			objEl->LinkEndChild(el);
		}

		// SkyCubeMap (MonoBehaviour)
		if (auto sky = obj->GetComponent<SkyCubeMap>())
		{
			XMLElement* el = doc->NewElement("SkyCubeMap");
			WriteWstrAttr(el, "file", sky->GetFileName());
			objEl->LinkEndChild(el);
		}

		root->LinkEndChild(objEl);
	}
}

bool SceneSerializer::Save(const wstring& path)
{
	// 대상 폴더 보장 (다이얼로그 외 직접 호출 경로 대비)
	std::error_code ec;
	filesystem::create_directories(filesystem::path(path).parent_path(), ec);

	tinyxml2::XMLDocument doc;
	doc.LinkEndChild(doc.NewDeclaration());

	XMLElement* root = doc.NewElement("Scene");
	root->SetAttribute("version", 1);
	doc.LinkEndChild(root);

	int32 count = 0;
	for (auto& [id, obj] : CUR_SCENE->GetCreatedObjects())
	{
		if (obj == nullptr || obj->IsEditorInternal())
			continue;

		WriteGameObject(root, obj);
		count++;
	}

	if (doc.SaveFile(Utils::ToString(path).c_str()) != XML_SUCCESS)
	{
		ADDLOG("Save Scene FAILED : " + Utils::ToString(path), LogFilter::Error);
		return false;
	}

	ADDLOG("Save Scene : " + Utils::ToString(path) + " (" + std::to_string(count) + " objects)", LogFilter::Info);
	return true;
}

bool SceneSerializer::Load(const wstring& path)
{
	// 커밋 102 에서 구현
	(void)path;
	return false;
}
