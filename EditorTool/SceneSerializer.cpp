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
#include "HlslShader.h"
#include "Light.h"
#include "Camera.h"
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
		objEl->SetAttribute("id", (unsigned)obj->GetId());

		// 계층 — 부모가 직렬화 대상(비-내부)이면 parent id 기록
		if (auto tr = obj->GetTransform())
		{
			if (auto parentTr = tr->GetParent())
			{
				auto parentObj = parentTr->GetGameObject();
				if (parentObj != nullptr && parentObj->IsEditorInternal() == false)
					objEl->SetAttribute("parent", (unsigned)parentObj->GetId());
			}
		}

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

		// Camera (게임 카메라 — 에디터 카메라는 internal 플래그로 이미 제외)
		if (auto cam = obj->GetCamera())
		{
			XMLElement* el = doc->NewElement("Camera");
			el->SetAttribute("projection", (int)cam->GetProjectionType());
			el->SetAttribute("fov", cam->GetFov());
			el->SetAttribute("near", cam->GetNear());
			el->SetAttribute("far", cam->GetFar());
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

namespace
{
	// 현재 씬 → XML 문서 (Save/SaveToString 공용)
	int32 BuildSceneDocument(tinyxml2::XMLDocument& doc)
	{
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
		return count;
	}
}

bool SceneSerializer::SaveToString(string& out)
{
	tinyxml2::XMLDocument doc;
	BuildSceneDocument(doc);

	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);
	out.assign(printer.CStr(), printer.CStrSize() > 0 ? printer.CStrSize() - 1 : 0);
	return true;
}

bool SceneSerializer::Save(const wstring& path)
{
	// 대상 폴더 보장 (다이얼로그 외 직접 호출 경로 대비)
	std::error_code ec;
	filesystem::create_directories(filesystem::path(path).parent_path(), ec);

	tinyxml2::XMLDocument doc;
	int32 count = BuildSceneDocument(doc);

	if (doc.SaveFile(Utils::ToString(path).c_str()) != XML_SUCCESS)
	{
		ADDLOG("Save Scene FAILED : " + Utils::ToString(path), LogFilter::Error);
		return false;
	}

	ADDLOG("Save Scene : " + Utils::ToString(path) + " (" + std::to_string(count) + " objects)", LogFilter::Info);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────
// 로드
// ─────────────────────────────────────────────────────────────────────────

namespace
{
	Vec3 ReadVec3(tinyxml2::XMLElement* parent, const char* name, const Vec3& fallback = Vec3::Zero)
	{
		tinyxml2::XMLElement* el = parent->FirstChildElement(name);
		if (el == nullptr)
			return fallback;
		return Vec3(el->FloatAttribute("x"), el->FloatAttribute("y"), el->FloatAttribute("z"));
	}

	Color ReadColor(tinyxml2::XMLElement* parent, const char* name, const Color& fallback)
	{
		tinyxml2::XMLElement* el = parent->FirstChildElement(name);
		if (el == nullptr)
			return fallback;
		return Color(el->FloatAttribute("r"), el->FloatAttribute("g"), el->FloatAttribute("b"), el->FloatAttribute("a"));
	}

	wstring ReadWstrAttr(tinyxml2::XMLElement* el, const char* name)
	{
		const char* value = el->Attribute(name);
		return value ? Utils::ToWString(value) : L"";
	}

	// 머티리얼 복원 — MaterialRef(공유 캐시) 또는 인라인
	shared_ptr<Material> ReadMaterialEl(tinyxml2::XMLElement* parent)
	{
		if (tinyxml2::XMLElement* refEl = parent->FirstChildElement("MaterialRef"))
		{
			wstring matPath = ReadWstrAttr(refEl, "path"); // <풀경로>.mat
			wstring matKey = Utils::ToMaterialKey(matPath);

			shared_ptr<Material> material = RESOURCES->Get<Material>(matKey);
			if (material == nullptr)
			{
				material = make_shared<Material>();
				// Material::Load 가 .mat 을 붙이므로 확장자 제거 후 전달
				material->Load(matPath.substr(0, matPath.size() - 4));
				RESOURCES->Add(matKey, material);
			}
			return material;
		}

		tinyxml2::XMLElement* matEl = parent->FirstChildElement("Material");
		if (matEl == nullptr)
			return nullptr;

		shared_ptr<Material> material = make_shared<Material>();
		material->SetHlslShader(RESOURCES->Get<HlslShader>(L"Standard_HLSL"));

		MaterialDesc& desc = material->GetMaterialDesc();
		desc.useTexture = matEl->IntAttribute("useTexture");
		desc.useAlphaclip = matEl->IntAttribute("useAlphaclip");
		desc.useSsao = matEl->IntAttribute("useSsao");
		desc.lightCount = matEl->IntAttribute("lightCount");
		desc.roughness = matEl->FloatAttribute("roughness");
		desc.metallic = matEl->FloatAttribute("metallic");
		material->SetRenderQueue((RenderQueue)matEl->IntAttribute("renderQueue"));

		desc.ambient = ReadColor(matEl, "Ambient", desc.ambient);
		desc.diffuse = ReadColor(matEl, "Diffuse", desc.diffuse);
		desc.specular = ReadColor(matEl, "Specular", desc.specular);
		desc.emissive = ReadColor(matEl, "Emissive", desc.emissive);

		if (tinyxml2::XMLElement* maps = matEl->FirstChildElement("Maps"))
		{
			wstring diffuse = ReadWstrAttr(maps, "diffuse");
			wstring normal = ReadWstrAttr(maps, "normal");
			wstring specular = ReadWstrAttr(maps, "specular");

			if (!diffuse.empty())  material->SetDiffuseMap(RESOURCES->GetOrAddTexture(diffuse, diffuse));
			if (!normal.empty())   material->SetNormalMap(RESOURCES->GetOrAddTexture(normal, normal));
			if (!specular.empty()) material->SetSpecularMap(RESOURCES->GetOrAddTexture(specular, specular));
		}

		return material;
	}

	// 모델 로드 (Load 1회 내 캐시 — 같은 모델 다수 배치 대응)
	shared_ptr<Model> LoadModel(const wstring& relPath, tinyxml2::XMLElement* animatorEl,
		map<wstring, shared_ptr<Model>>& cache)
	{
		auto found = cache.find(relPath);
		if (found != cache.end())
			return found->second;

		shared_ptr<Model> model = make_shared<Model>();
		model->ReadModel(relPath);

		// .mmat(바이너리) 우선, 레거시 .xml 폴백
		wstring mmatPath = L"../Resources/Assets/Models/" + relPath + L".mmat";
		if (filesystem::exists(mmatPath))
			model->ReadMaterial(relPath);
		else
			model->ReadMaterialByXml(relPath);

		// 클립 — <Clip file="Idle.clip"/> → ReadAnimation("<모델폴더>/Idle")
		if (animatorEl)
		{
			wstring folder = relPath.substr(0, relPath.find(L'/'));
			for (tinyxml2::XMLElement* clipEl = animatorEl->FirstChildElement("Clip");
				clipEl; clipEl = clipEl->NextSiblingElement("Clip"))
			{
				wstring file = ReadWstrAttr(clipEl, "file");
				wstring stem = filesystem::path(file).stem().wstring();
				model->ReadAnimation(folder + L"/" + stem);
			}
		}

		cache[relPath] = model;
		return model;
	}

	shared_ptr<GameObject> LoadGameObject(tinyxml2::XMLElement* objEl, map<wstring, shared_ptr<Model>>& modelCache)
	{
		auto obj = make_shared<GameObject>();
		obj->SetObjectName(ReadWstrAttr(objEl, "name"));
		obj->SetLayerIndex((uint8)objEl->IntAttribute("layer"));

		if (tinyxml2::XMLElement* trEl = objEl->FirstChildElement("Transform"))
		{
			auto tr = obj->GetOrAddTransform();
			tr->SetLocalPosition(ReadVec3(trEl, "Position"));
			tr->SetLocalRotation(ReadVec3(trEl, "Rotation"));
			tr->SetLocalScale(ReadVec3(trEl, "Scale", Vec3(1.f, 1.f, 1.f)));
		}

		bool hasSky = objEl->FirstChildElement("SkyCubeMap") != nullptr;

		// MeshRenderer — SkyCubeMap 이 있으면 Init 이 자체 생성하므로 스킵
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("MeshRenderer"); el && !hasSky)
		{
			wstring meshName = ReadWstrAttr(el, "mesh");
			shared_ptr<Mesh> mesh = RESOURCES->Get<Mesh>(meshName);

			if (mesh == nullptr)
			{
				ADDLOG("Load Scene : unknown mesh '" + Utils::ToString(meshName) + "' skipped", LogFilter::Warn);
			}
			else
			{
				auto mr = make_shared<MeshRenderer>();
				mr->SetMesh(mesh);
				if (auto material = ReadMaterialEl(el))
					mr->SetMaterial(material);
				mr->SetTechnique((uint8)el->IntAttribute("technique"));
				obj->AddComponent(mr);
			}
		}

		// ModelRenderer
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("ModelRenderer"))
		{
			wstring relPath = ReadWstrAttr(el, "model");
			if (!relPath.empty())
			{
				auto mr = make_shared<ModelRenderer>();
				mr->SetModel(LoadModel(relPath, nullptr, modelCache));
				mr->SetTechnique((uint8)el->IntAttribute("technique"));
				obj->AddComponent(mr);
			}
		}

		// ModelAnimator
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("ModelAnimator"))
		{
			wstring relPath = ReadWstrAttr(el, "model");
			if (!relPath.empty())
			{
				auto ma = make_shared<ModelAnimator>();
				ma->SetModel(LoadModel(relPath, el, modelCache));
				ma->SetTechnique((uint8)el->IntAttribute("technique"));
				ma->GetTweenDesc().curr.animIndex = el->IntAttribute("animIndex");
				obj->AddComponent(ma);
			}
		}

		// Light
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("Light"))
		{
			auto light = make_shared<Light>();

			LightDesc desc;
			desc.ambient = ReadColor(el, "Ambient", desc.ambient);
			desc.diffuse = ReadColor(el, "Diffuse", desc.diffuse);
			desc.specular = ReadColor(el, "Specular", desc.specular);
			desc.direction = ReadVec3(el, "Direction", Vec3(0.f, -1.f, 0.f));
			desc.intensity = el->FloatAttribute("intensity", 1.f);

			light->SetLightType((LightType)el->IntAttribute("type"));
			light->SetLightDesc(desc);
			light->SetIntensityValue(desc.intensity);
			light->SetRange(el->FloatAttribute("range", 25.f));
			light->SetSpotAngleDeg(el->FloatAttribute("spotAngle", 30.f));
			light->SetAttenuation(ReadVec3(el, "Attenuation", Vec3(1.f, 0.09f, 0.032f)));

			obj->AddComponent(light);
		}

		// Camera (게임 카메라)
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("Camera"))
		{
			auto cam = make_shared<Camera>();
			cam->SetProjectionType((ProjectionType)el->IntAttribute("projection"));
			cam->SetFOV(el->FloatAttribute("fov", XM_PI / 4.f));
			cam->SetNear(el->FloatAttribute("near", 0.01f));
			cam->SetFar(el->FloatAttribute("far", 1000.f));
			obj->AddComponent(cam);
		}

		// Terrain (머티리얼은 원 생성 경로와 동일하게 DefaultMaterial 클론)
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("Terrain"))
		{
			TerrainInfo info;
			info.heightMapFilename = ReadWstrAttr(el, "heightMap");
			info.blendMapFilename = ReadWstrAttr(el, "blendMap");
			info.heightScale = el->FloatAttribute("heightScale");
			info.heightmapWidth = el->UnsignedAttribute("width");
			info.heightmapHeight = el->UnsignedAttribute("height");
			info.cellSpacing = el->FloatAttribute("cellSpacing");

			for (tinyxml2::XMLElement* layerEl = el->FirstChildElement("Layer");
				layerEl; layerEl = layerEl->NextSiblingElement("Layer"))
			{
				info.layerMapFilenames.push_back(ReadWstrAttr(layerEl, "file"));
			}

			auto terrain = make_shared<Terrain>();
			obj->AddComponent(terrain);

			auto mat = RESOURCES->Get<Material>(L"DefaultMaterial")->Clone();
			mat->GetMaterialDesc().roughness = 0.9f; // 지면 — 거의 무광
			terrain->Init(info, mat);
		}

		// ParticleSystem
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("ParticleSystem"))
		{
			std::vector<wstring> texNames;
			for (tinyxml2::XMLElement* texEl = el->FirstChildElement("Texture");
				texEl; texEl = texEl->NextSiblingElement("Texture"))
			{
				texNames.push_back(ReadWstrAttr(texEl, "file"));
			}

			auto ps = make_shared<ParticleSystem>();
			obj->AddComponent(ps);
			ps->Init(el->IntAttribute("type"), texNames, el->UnsignedAttribute("max"));

			// Init 의 타입별 기본값 위에 저장값 덮어쓰기
			ps->SetAccel(ReadVec3(el, "Accel"));
			ps->SetEmitDir(ReadVec3(el, "EmitDir", Vec3::Up));
			ps->SetEmitInterval(el->FloatAttribute("emitInterval"));
			ps->SetLifetime(el->FloatAttribute("lifetime"));
			ps->SetInitialSpeed(el->FloatAttribute("initialSpeed"));
			ps->SetParticleSize(Vec2(el->FloatAttribute("sizeX"), el->FloatAttribute("sizeY")));
		}

		// SkyCubeMap — Init 이 MeshRenderer/머티리얼을 자체 구성
		if (tinyxml2::XMLElement* el = objEl->FirstChildElement("SkyCubeMap"))
		{
			auto sky = make_shared<SkyCubeMap>();
			obj->AddComponent(sky);
			sky->Init(ReadWstrAttr(el, "file"));
		}

		CUR_SCENE->Add(obj);
		return obj;
	}
}

void SceneSerializer::Clear()
{
	// 순회 중 Remove 는 맵을 변형하므로 목록 복사 후 제거
	vector<shared_ptr<GameObject>> toRemove;
	for (auto& [id, obj] : CUR_SCENE->GetCreatedObjects())
	{
		if (obj != nullptr && obj->IsEditorInternal() == false)
			toRemove.push_back(obj);
	}

	for (auto& obj : toRemove)
		CUR_SCENE->Remove(obj);

	TOOL->SetSelectedObjH(-1);
}

namespace
{
	// XML 문서 → 씬 재구성 (Load/LoadFromString 공용). 반환 = 생성 오브젝트 수 (-1 = 실패)
	int32 LoadFromDocument(tinyxml2::XMLDocument& doc)
	{
		tinyxml2::XMLElement* root = doc.FirstChildElement("Scene");
		if (root == nullptr)
			return -1;

		SceneSerializer::Clear();

		map<wstring, shared_ptr<Model>> modelCache;
		map<uint32, shared_ptr<GameObject>> bySavedId;
		vector<pair<shared_ptr<GameObject>, uint32>> pendingParent;

		int32 count = 0;
		for (tinyxml2::XMLElement* objEl = root->FirstChildElement("GameObject");
			objEl; objEl = objEl->NextSiblingElement("GameObject"))
		{
			shared_ptr<GameObject> obj = LoadGameObject(objEl, modelCache);
			count++;

			uint32 savedId = objEl->UnsignedAttribute("id");
			if (savedId != 0)
				bySavedId[savedId] = obj;

			uint32 parentId = objEl->UnsignedAttribute("parent");
			if (parentId != 0)
				pendingParent.push_back({ obj, parentId });
		}

		// 2패스 — 계층 연결 (저장된 로컬 값을 보존해야 하므로 KeepWorld 가 아닌 로컬 보존 연결)
		for (auto& [child, parentId] : pendingParent)
		{
			auto found = bySavedId.find(parentId);
			if (found == bySavedId.end())
				continue;

			auto childTr = child->GetTransform();
			auto parentTr = found->second->GetTransform();
			if (childTr == nullptr || parentTr == nullptr)
				continue;

			childTr->SetParent(parentTr);
			parentTr->AddChild(childTr);
			childTr->UpdateTransform();
		}

		return count;
	}
}

namespace
{
	void CollectSubtree(shared_ptr<GameObject> obj, vector<shared_ptr<GameObject>>& out)
	{
		out.push_back(obj);
		if (auto tr = obj->GetTransform())
		{
			for (auto& child : tr->GetChildren())
			{
				if (auto childObj = child->GetGameObject())
					CollectSubtree(childObj, out);
			}
		}
	}
}

// 직렬화 가능한 컴포넌트 기준 복제 (XML 왕복) — 자식 서브트리 포함
int64 SceneSerializer::Duplicate(int64 objectId)
{
	shared_ptr<GameObject> src = CUR_SCENE->GetCreatedObject((int32)objectId);
	if (src == nullptr || src->IsEditorInternal())
		return -1;

	vector<shared_ptr<GameObject>> subtree;
	CollectSubtree(src, subtree);

	tinyxml2::XMLDocument doc;
	XMLElement* root = doc.NewElement("Scene");
	doc.LinkEndChild(root);
	for (auto& obj : subtree)
		WriteGameObject(root, obj);

	// 재로드 — 서브트리 내부 계층만 연결 (루트의 parent 는 bySavedId 에 없어 자동 스킵)
	map<wstring, shared_ptr<Model>> modelCache;
	map<uint32, shared_ptr<GameObject>> bySavedId;
	vector<pair<shared_ptr<GameObject>, uint32>> pendingParent;
	shared_ptr<GameObject> newRoot;

	for (XMLElement* el = root->FirstChildElement("GameObject"); el; el = el->NextSiblingElement("GameObject"))
	{
		shared_ptr<GameObject> obj = LoadGameObject(el, modelCache);
		if (newRoot == nullptr)
			newRoot = obj;

		uint32 savedId = el->UnsignedAttribute("id");
		if (savedId != 0)
			bySavedId[savedId] = obj;

		uint32 parentId = el->UnsignedAttribute("parent");
		if (parentId != 0)
			pendingParent.push_back({ obj, parentId });
	}

	for (auto& [child, parentId] : pendingParent)
	{
		auto found = bySavedId.find(parentId);
		if (found == bySavedId.end())
			continue;

		auto childTr = child->GetTransform();
		auto parentTr = found->second->GetTransform();
		if (childTr == nullptr || parentTr == nullptr)
			continue;

		childTr->SetParent(parentTr);
		parentTr->AddChild(childTr);
		childTr->UpdateTransform();
	}

	if (newRoot == nullptr)
		return -1;

	// 복제 루트를 원본과 같은 부모 밑으로 (로컬 값 유지 → 같은 위치)
	if (auto srcTr = src->GetTransform())
	{
		if (auto parentTr = srcTr->GetParent())
		{
			if (auto newTr = newRoot->GetTransform())
			{
				newTr->SetParent(parentTr);
				parentTr->AddChild(newTr);
				newTr->UpdateTransform();
			}
		}
	}

	// 루트 이름 고유화 + 이름 맵 복구 (Load 의 Add 가 원본 이름 키를 덮어썼음)
	{
		wstring base = src->GetObjectName();
		int32 n = 1;
		wstring name;
		do { name = base + L" (" + std::to_wstring(n++) + L")"; }
		while (CUR_SCENE->FindCreatedObjectByName(name) != nullptr);
		newRoot->SetObjectName(name);

		CUR_SCENE->RegisterName(newRoot);
		for (auto& original : subtree)
			CUR_SCENE->RegisterName(original);
	}

	return newRoot->GetId();
}

bool SceneSerializer::LoadFromString(const string& xml)
{
	tinyxml2::XMLDocument doc;
	if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
		return false;

	return LoadFromDocument(doc) >= 0;
}

bool SceneSerializer::Load(const wstring& path)
{
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(Utils::ToString(path).c_str()) != tinyxml2::XML_SUCCESS)
	{
		ADDLOG("Load Scene FAILED : " + Utils::ToString(path), LogFilter::Error);
		return false;
	}

	int32 count = LoadFromDocument(doc);
	if (count < 0)
	{
		ADDLOG("Load Scene FAILED : <Scene> root missing", LogFilter::Error);
		return false;
	}

	ADDLOG("Load Scene : " + Utils::ToString(path) + " (" + std::to_string(count) + " objects)", LogFilter::Info);
	return true;
}
