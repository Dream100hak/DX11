#include "D3D12Device.h"
#include "EditorWindows.h"
#include "Component.h"
#include "Transform.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "ModelAnimator.h"
#include "Collider.h"
#include "ParticleSystem.h"
#include "Terrain.h"
#include "Foliage.h"
#include "Billboard.h"
#include "Scripts.h"
#include "GeometryHelper.h"
#include "ResourceManager.h"
#include "Camera.h"
#include "Light.h"
#include "MonoBehaviour.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"
#include "FbxConverter.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

namespace fs = std::filesystem;
#include "EditorUtil.h"   // WToUtf8 / BuildPrim (공용 헬퍼)

// 씬 직렬화 — Save/Load Scene (.scene XML) (D3D12Editor.cpp 에서 분리)
static std::wstring QuickScenePath(const std::wstring& root)
{
	fs::path dir = fs::path(root) / L"Scenes";
	std::error_code ec; fs::create_directories(dir, ec);
	return (dir / L"quick.rtscene").wstring();
}

// GameObject 의 스크립트(MonoBehaviour) 직렬화 — 각 오브젝트 블록 끝에 mb 라인 추가
static void WriteScripts(std::ofstream& f, const shared_ptr<GameObject>& obj)
{
	for (auto& s : obj->GetMonoBehaviours())
	{
		if (!s) continue;
		std::ostringstream os; s->Serialize(os);
		f << "mb " << s->TypeName() << ' ' << os.str() << '\n';
	}
}

void D3D12Device::SaveScene() { SaveSceneTo(QuickScenePath(_assetRoot)); }

void D3D12Device::SaveSceneTo(const std::wstring& path)
{
	std::ofstream f(path);
	if (!f) { Log("Save FAILED: " + WToUtf8(path)); return; }
	f << "cam " << _camera.pos.x << ' ' << _camera.pos.y << ' ' << _camera.pos.z << ' ' << _camera.yaw << ' ' << _camera.pitch << '\n';
	f << "sun " << _lightIntensity << ' ' << _lightAngle << ' ' << (_lightAnimate ? 1 : 0) << '\n';
	f << "point " << (_pointOn ? 1 : 0) << ' ' << _pointPos.x << ' ' << _pointPos.y << ' ' << _pointPos.z
	  << ' ' << _pointColor.x << ' ' << _pointColor.y << ' ' << _pointColor.z << ' ' << _pointIntensity << ' ' << _pointRadius << '\n';
	f << "gi " << _giStrength << ' ' << _ambient << ' ' << _exposure << '\n';
	f << "model " << WToUtf8((_scene._modelDir + _scene._modelStem + L".mesh")) << '\n';

	// ── 멀티 오브젝트: 씬그래프의 MeshRenderer GameObject 들 (트랜스폼 + 머티리얼 + 텍스처) ──
	int meshCount = 0, lightCount = 0, animCount = 0, foliageCount = 0;
	if (_gameScene)
	{
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			auto mr = obj->GetMeshRenderer();
			if (!mr) continue; // 라이트/모델은 위 스칼라 라인으로 영속
			if (obj->GetComponent<Terrain>()) continue; // 터레인은 전용 tobj 블록으로 영속
			auto t = obj->GetTransform(); if (!t) continue;
			Vec3 p = t->GetLocalPosition(), r = t->GetLocalRotation(), sc = t->GetLocalScale();
			Material& mat = mr->GetMaterial();
			f << "mobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "mprim " << (int)mr->GetPrim() << '\n'; // 0=None(매칭만), 1=Cube,2=Sphere,3=Plane(재생성)
			std::wstring parentName;
			if (auto pt = t->GetParent()) if (auto pgo = pt->GetGameObject()) parentName = pgo->GetObjectName();
			f << "mpar " << (parentName.empty() ? std::string("-") : WToUtf8(parentName)) << '\n';
			f << "mxf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << r.x << ' ' << r.y << ' ' << r.z
			  << ' ' << sc.x << ' ' << sc.y << ' ' << sc.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
			if (!mat._path.empty())
				f << "mref " << WToUtf8(mat._path) << '\n'; // 공유 .mat 자산 참조
			else
			{
				f << "mmat " << mat._diffuse.x << ' ' << mat._diffuse.y << ' ' << mat._diffuse.z
				  << ' ' << mat._metallic << ' ' << mat._roughness << ' ' << mat._emissive << '\n';
				f << "mtex " << (mat._diffuseTex.empty() ? std::string("-") : WToUtf8(mat._diffuseTex)) << '\n';
			}
			if (auto bc = obj->GetComponent<AABBBoxCollider>())
				f << "mcol 1 " << bc->_center.x << ' ' << bc->_center.y << ' ' << bc->_center.z
				  << ' ' << bc->_extents.x << ' ' << bc->_extents.y << ' ' << bc->_extents.z << '\n';
			else if (auto sp = obj->GetComponent<SphereCollider>())
				f << "mcol 0 " << sp->_center.x << ' ' << sp->_center.y << ' ' << sp->_center.z
				  << ' ' << sp->_radius << " 0 0\n";
			WriteScripts(f, obj);
			++meshCount;
		}

		// 추가 라이트(고정 3개 제외) — Light 컴포넌트 보유 GameObject
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			if (obj == _sunObj || obj == _ptObj || obj == _spotObj) continue; // 고정 라이트는 스칼라로 영속
			auto l = obj->GetLight(); if (!l) continue;
			auto t = obj->GetTransform(); Vec3 p = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
			std::wstring parentName;
			if (t) if (auto pt = t->GetParent()) if (auto pgo = pt->GetGameObject()) parentName = pgo->GetObjectName();
			f << "lobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "lprm " << (int)l->_lightType << ' ' << l->_color.x << ' ' << l->_color.y << ' ' << l->_color.z
			  << ' ' << l->_intensity << ' ' << l->_range << ' ' << l->_spotAngleDeg << ' ' << (l->_enabled ? 1 : 0) << '\n';
			f << "ldir " << l->_direction.x << ' ' << l->_direction.y << ' ' << l->_direction.z << '\n';
			f << "lxf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
			f << "lpar " << (parentName.empty() ? std::string("-") : WToUtf8(parentName)) << '\n';
			WriteScripts(f, obj);
			++lightCount;
		}

		// 애니메이션 모델 (ModelAnimator)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			auto an = obj->GetModelAnimator(); if (!an) continue;
			auto t = obj->GetTransform(); if (!t) continue;
			Vec3 p = t->GetLocalPosition(), r = t->GetLocalRotation(), sc = t->GetLocalScale();
			std::wstring parentName;
			if (auto pt = t->GetParent()) if (auto pgo = pt->GetGameObject()) parentName = pgo->GetObjectName();
			std::wstring meshPath = an->MeshDir() + an->MeshStem() + L".mesh";
			f << "aobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "apath " << WToUtf8(meshPath) << '\n';
			f << "aclip " << an->GetClipIndex() << ' ' << an->GetSpeed() << ' ' << (an->IsPlaying() ? 1 : 0) << '\n';
			f << "axf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << r.x << ' ' << r.y << ' ' << r.z
			  << ' ' << sc.x << ' ' << sc.y << ' ' << sc.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
			f << "apar " << (parentName.empty() ? std::string("-") : WToUtf8(parentName)) << '\n';
			WriteScripts(f, obj);
			++animCount;
		}

		// 파티클 시스템
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			auto ps = std::dynamic_pointer_cast<ParticleSystem>(obj->GetRenderer()); if (!ps) continue;
			auto t = obj->GetTransform(); Vec3 p = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
			std::wstring parentName;
			if (t) if (auto pt = t->GetParent()) if (auto pgo = pt->GetGameObject()) parentName = pgo->GetObjectName();
			f << "pobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "pprm " << ps->_mode << ' ' << (ps->_emitting ? 1 : 0) << ' ' << ps->_rate << ' ' << ps->_life
			  << ' ' << ps->_speed << ' ' << ps->_gravity << ' ' << ps->_size
			  << ' ' << ps->_color.x << ' ' << ps->_color.y << ' ' << ps->_color.z << '\n';
			// 확장 파라미터 (방출형태/블렌드/버스트/곡선)
			f << "pprm2 " << ps->_shape << ' ' << ps->_shapeRadius << ' ' << ps->_coneAngle
			  << ' ' << ps->_boxSize.x << ' ' << ps->_boxSize.y << ' ' << ps->_boxSize.z
			  << ' ' << ps->_dir.x << ' ' << ps->_dir.y << ' ' << ps->_dir.z << ' ' << ps->_spread
			  << ' ' << ps->_blend << ' ' << ps->_soft << ' ' << ps->_sizeEnd << ' ' << ps->_fadeIn << ' ' << ps->_burst
			  << ' ' << ps->_colorEnd.x << ' ' << ps->_colorEnd.y << ' ' << ps->_colorEnd.z << '\n';
			f << "pxf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
			f << "ppar " << (parentName.empty() ? std::string("-") : WToUtf8(parentName)) << '\n';
			WriteScripts(f, obj);
		}

		// 게임 카메라 (Camera 컴포넌트 GameObject)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			auto cam = obj->GetCamera(); if (!cam) continue;
			auto t = obj->GetTransform(); if (!t) continue;
			Vec3 p = t->GetLocalPosition(), r = t->GetLocalRotation();
			std::wstring parentName;
			if (auto pt = t->GetParent()) if (auto pgo = pt->GetGameObject()) parentName = pgo->GetObjectName();
			f << "cobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "cprm " << cam->_fov << ' ' << cam->_near << ' ' << cam->_far << ' ' << (int)cam->_projType << '\n';
			f << "cxf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << r.x << ' ' << r.y << ' ' << r.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
			f << "cpar " << (parentName.empty() ? std::string("-") : WToUtf8(parentName)) << '\n';
			WriteScripts(f, obj);
		}

		// 터레인 (Terrain 컴포넌트 GameObject) — 하이트맵은 사이드카 .r32 로 자동 저장
		{
			fs::path scenePath(path);
			std::wstring sceneDir = scenePath.has_parent_path() ? (scenePath.parent_path().wstring() + L"\\") : L"";
			for (auto& kv : _gameScene->GetCreatedObjects())
			{
				auto& obj = kv.second;
				if (!obj || obj->IsEditorInternal()) continue;
				auto terr = obj->GetComponent<Terrain>(); if (!terr) continue;
				auto t = obj->GetTransform(); Vec3 p = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
				// 하이트맵 사이드카 경로: <sceneDir><terrainName>.r32
				std::wstring hmPath = sceneDir + obj->GetObjectName() + L".r32";
				terr->SaveHeightmap(hmPath);
				f << "tobj " << WToUtf8(obj->GetObjectName()) << '\n';
				f << "tprm " << terr->GridN() << ' ' << terr->CellSize() << '\n';
				f << "thm " << WToUtf8(hmPath) << '\n';
				f << "txf " << p.x << ' ' << p.y << ' ' << p.z << ' ' << (obj->IsActive() ? 1 : 0) << '\n';
				WriteScripts(f, obj);
			}
		}

		// 식생 (Foliage 렌더러 GameObject) — 생성 파라미터만 저장(로드 시 결정적 재생성)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj->IsEditorInternal()) continue;
			auto fol = std::dynamic_pointer_cast<Foliage>(obj->GetRenderer()); if (!fol) continue;
			// 소유 터레인 이름 = "<terrainName>_Foliage" 에서 역산
			std::wstring fname = obj->GetObjectName();
			std::wstring owner = (fname.size() > 8 && fname.substr(fname.size() - 8) == L"_Foliage") ? fname.substr(0, fname.size() - 8) : L"";
			f << "fobj " << WToUtf8(obj->GetObjectName()) << '\n';
			f << "fown " << (owner.empty() ? std::string("-") : WToUtf8(owner)) << '\n';
			f << "fprm " << fol->GrassCount() << ' ' << fol->TreeCount() << ' ' << fol->GrassSize() << ' ' << fol->Seed() << '\n';
			++foliageCount;
		}
	}
	f.flush();
	Log("Scene saved: " + WToUtf8(path) + "  (" + std::to_string(meshCount) + " mesh, "
		+ std::to_string(lightCount) + " light, " + std::to_string(animCount) + " anim, "
		+ std::to_string(foliageCount) + " foliage)");
}

void D3D12Device::LoadScene() { LoadSceneFrom(QuickScenePath(_assetRoot)); }

void D3D12Device::LoadSceneFrom(const std::wstring& path)
{
	std::ifstream f(path);
	if (!f) { Log("Open FAILED (no scene file): " + WToUtf8(path)); return; }
	std::string line;
	std::string modelUtf8;
	shared_ptr<GameObject> curObj; // 현재 mobj 블록 대상
	shared_ptr<GameObject> curLight; std::wstring curLightName; // 현재 lobj 블록 대상
	shared_ptr<GameObject> curAnim;  std::wstring curAnimName;  // 현재 aobj 블록 대상
	shared_ptr<GameObject> curPart;  std::wstring curPartName;  // 현재 pobj 블록 대상
	shared_ptr<GameObject> curTerrain; std::wstring curTerrainName; // 현재 tobj 블록 대상
	shared_ptr<GameObject> curFoliage; std::wstring curFoliageName, curFoliageOwner; // 현재 fobj 블록 대상
	shared_ptr<GameObject> curAny;   // 가장 최근 블록 오브젝트 (스크립트 mb 적용 대상)
	std::wstring curName;          // 현재 블록 오브젝트 이름 (없으면 재생성용)
	std::vector<std::pair<std::wstring, std::wstring>> parentLinks; // (child, parent) — 전부 파싱 후 링크
	auto findByName = [&](const std::wstring& wname) -> shared_ptr<GameObject>
	{
		if (!_gameScene) return nullptr;
		for (auto& kv : _gameScene->GetCreatedObjects())
			if (kv.second && kv.second->GetObjectName() == wname) return kv.second;
		return nullptr;
	};
	while (std::getline(f, line))
	{
		std::istringstream s(line); std::string tag; s >> tag;
		if (tag == "cam") s >> _camera.pos.x >> _camera.pos.y >> _camera.pos.z >> _camera.yaw >> _camera.pitch;
		else if (tag == "sun") { int an; s >> _lightIntensity >> _lightAngle >> an; _lightAnimate = an != 0; }
		else if (tag == "point") { int on; s >> on >> _pointPos.x >> _pointPos.y >> _pointPos.z >> _pointColor.x >> _pointColor.y >> _pointColor.z >> _pointIntensity >> _pointRadius; _pointOn = on != 0; }
		else if (tag == "gi") s >> _giStrength >> _ambient >> _exposure;
		else if (tag == "model") { std::getline(s >> std::ws, modelUtf8); }
		// ── 멀티 오브젝트 ──
		else if (tag == "mobj") { std::string nm; std::getline(s >> std::ws, nm); curName = Utf8ToW(nm); curObj = findByName(curName); curAny = curObj; }
		else if (tag == "mprim") { // 없는 오브젝트면 프리미티브 재생성 (스폰 오브젝트 영속)
			int pk = 0; s >> pk;
			if (!curObj && pk != 0 && !curName.empty()) {
				vector<Vtx> v; vector<uint32> idx; MeshPrim prim = (MeshPrim)pk; BuildPrim(prim, v, idx);
				curObj = SpawnMeshObject(curName, v, idx, Vec3{ 0,0,0 }, prim, false); // 정확한 이름, 자동선택 안 함
				curAny = curObj;
			}
		}
		else if (tag == "mpar" && curObj) {
			std::string pn; std::getline(s >> std::ws, pn);
			if (pn != "-" && !pn.empty()) parentLinks.push_back({ curName, Utf8ToW(pn) });
		}
		else if (tag == "mxf" && curObj) {
			Vec3 p, r, sc; s >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z >> sc.x >> sc.y >> sc.z;
			if (auto t = curObj->GetTransform()) { t->SetLocalScale(sc); t->SetLocalRotation(r); t->SetLocalPosition(p); }
			int act = 1; if (s >> act) curObj->SetActive(act != 0); // 구버전 파일 호환(없으면 활성)
		}
		else if (tag == "mmat" && curObj) {
			if (auto mr = curObj->GetMeshRenderer()) { Material& m = mr->GetMaterial();
				s >> m._diffuse.x >> m._diffuse.y >> m._diffuse.z >> m._metallic >> m._roughness >> m._emissive; }
		}
		else if (tag == "mtex" && curObj) {
			std::string tp; std::getline(s >> std::ws, tp);
			if (auto mr = curObj->GetMeshRenderer())
				mr->GetMaterial()._diffuseTex = (tp == "-" || tp.empty()) ? L"" : Utf8ToW(tp);
		}
		else if (tag == "mref" && curObj) { // 공유 .mat 자산
			std::string mp2; std::getline(s >> std::ws, mp2); std::wstring wp = Utf8ToW(mp2);
			if (auto mr = curObj->GetMeshRenderer())
			{
				auto shared = GET_SINGLE(ResourceManager)->Get<Material>(wp);
				if (!shared) { shared = LoadMaterial(wp); if (shared) GET_SINGLE(ResourceManager)->Add<Material>(wp, shared); }
				if (shared) mr->SetMaterialRef(shared);
			}
		}
		else if (tag == "mcol" && curObj) { // 콜라이더 (1=박스 / 0=구)
			int ct = 0; float cx, cy, cz, a, b, c2; s >> ct >> cx >> cy >> cz >> a >> b >> c2;
			if (!curObj->GetComponent<BaseCollider>())
			{
				if (ct == 1) { auto bc = make_shared<AABBBoxCollider>(); bc->_center = { cx,cy,cz }; bc->_extents = { a,b,c2 }; curObj->AddComponent(bc); }
				else { auto sc2 = make_shared<SphereCollider>(); sc2->_center = { cx,cy,cz }; sc2->_radius = a; curObj->AddComponent(sc2); }
			}
		}
		// ── 추가 라이트 ──
		else if (tag == "lobj") {
			std::string nm; std::getline(s >> std::ws, nm); curLightName = Utf8ToW(nm);
			curLight = findByName(curLightName);
			if (!curLight) { // 없으면 빈 GameObject + Light 생성
				curLight = make_shared<GameObject>();
				curLight->SetObjectName(curLightName);
				curLight->GetOrAddTransform();
				curLight->AddComponent(make_shared<Light>());
				_gameScene->Add(curLight);
			}
			else if (!curLight->GetLight()) curLight->AddComponent(make_shared<Light>());
			curAny = curLight;
		}
		else if (tag == "lprm" && curLight) {
			if (auto l = curLight->GetLight()) {
				int ty = 0, en = 1;
				s >> ty >> l->_color.x >> l->_color.y >> l->_color.z >> l->_intensity >> l->_range >> l->_spotAngleDeg >> en;
				l->_lightType = (LightType)ty; l->_enabled = en != 0;
			}
		}
		else if (tag == "ldir" && curLight) {
			if (auto l = curLight->GetLight()) s >> l->_direction.x >> l->_direction.y >> l->_direction.z;
		}
		else if (tag == "lxf" && curLight) {
			Vec3 p; s >> p.x >> p.y >> p.z; if (auto t = curLight->GetTransform()) t->SetLocalPosition(p);
			int act = 1; if (s >> act) curLight->SetActive(act != 0);
		}
		else if (tag == "lpar" && curLight) {
			std::string pn; std::getline(s >> std::ws, pn);
			if (pn != "-" && !pn.empty()) parentLinks.push_back({ curLightName, Utf8ToW(pn) });
		}
		// ── 애니메이션 모델 ──
		else if (tag == "aobj") { std::string nm; std::getline(s >> std::ws, nm); curAnimName = Utf8ToW(nm); curAnim = findByName(curAnimName); curAny = curAnim; }
		else if (tag == "apath") {
			std::string ap; std::getline(s >> std::ws, ap); std::wstring meshPath = Utf8ToW(ap);
			if (!curAnim) { // 없으면 생성 (정확한 이름)
				auto obj = make_shared<GameObject>();
				obj->SetObjectName(curAnimName); obj->GetOrAddTransform();
				auto an = make_shared<ModelAnimator>(); an->Bind(this);
				if (an->Load(meshPath)) { obj->AddComponent(an); _gameScene->Add(obj); curAnim = obj; }
				else Log("Load: animated model FAILED " + ap);
			}
			curAny = curAnim;
		}
		else if (tag == "aclip" && curAnim) {
			int ci = 0, pl = 1; float sp = 1.f; s >> ci >> sp >> pl;
			if (auto an = curAnim->GetModelAnimator()) { an->SetClipIndex(ci); an->SetSpeed(sp); an->SetPlaying(pl != 0); }
		}
		else if (tag == "axf" && curAnim) {
			Vec3 p, r, sc; s >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z >> sc.x >> sc.y >> sc.z;
			if (auto t = curAnim->GetTransform()) { t->SetLocalScale(sc); t->SetLocalRotation(r); t->SetLocalPosition(p); }
			int act = 1; if (s >> act) curAnim->SetActive(act != 0);
		}
		else if (tag == "apar" && curAnim) {
			std::string pn; std::getline(s >> std::ws, pn);
			if (pn != "-" && !pn.empty()) parentLinks.push_back({ curAnimName, Utf8ToW(pn) });
		}
		// ── 파티클 시스템 ──
		else if (tag == "pobj") {
			std::string nm; std::getline(s >> std::ws, nm); curPartName = Utf8ToW(nm);
			curPart = findByName(curPartName);
			if (!curPart) {
				curPart = make_shared<GameObject>(); curPart->SetObjectName(curPartName); curPart->GetOrAddTransform();
				curPart->AddComponent(make_shared<ParticleSystem>()); _gameScene->Add(curPart);
			}
			else if (!std::dynamic_pointer_cast<ParticleSystem>(curPart->GetRenderer())) curPart->AddComponent(make_shared<ParticleSystem>());
			curAny = curPart;
		}
		else if (tag == "pprm" && curPart) {
			if (auto ps = std::dynamic_pointer_cast<ParticleSystem>(curPart->GetRenderer())) {
				int em = 1; s >> ps->_mode >> em >> ps->_rate >> ps->_life >> ps->_speed >> ps->_gravity >> ps->_size
					>> ps->_color.x >> ps->_color.y >> ps->_color.z; ps->_emitting = em != 0;
			}
		}
		else if (tag == "pprm2" && curPart) {
			if (auto ps = std::dynamic_pointer_cast<ParticleSystem>(curPart->GetRenderer())) {
				s >> ps->_shape >> ps->_shapeRadius >> ps->_coneAngle
				  >> ps->_boxSize.x >> ps->_boxSize.y >> ps->_boxSize.z
				  >> ps->_dir.x >> ps->_dir.y >> ps->_dir.z >> ps->_spread
				  >> ps->_blend >> ps->_soft >> ps->_sizeEnd >> ps->_fadeIn >> ps->_burst
				  >> ps->_colorEnd.x >> ps->_colorEnd.y >> ps->_colorEnd.z;
			}
		}
		else if (tag == "pxf" && curPart) {
			Vec3 p; s >> p.x >> p.y >> p.z; if (auto t = curPart->GetTransform()) t->SetLocalPosition(p);
			int act = 1; if (s >> act) curPart->SetActive(act != 0);
		}
		else if (tag == "ppar" && curPart) {
			std::string pn; std::getline(s >> std::ws, pn);
			if (pn != "-" && !pn.empty()) parentLinks.push_back({ curPartName, Utf8ToW(pn) });
		}
		// ── 게임 카메라 ──
		else if (tag == "cobj") {
			std::string nm; std::getline(s >> std::ws, nm); curName = Utf8ToW(nm);
			curObj = findByName(curName);
			if (!curObj) { curObj = SpawnEmpty(curName, Vec3{ 0,0,0 }); if (curObj) curObj->SetObjectName(curName); }
			if (curObj && !curObj->GetCamera()) curObj->AddComponent(make_shared<Camera>());
			curAny = curObj;
		}
		else if (tag == "cprm" && curObj) {
			if (auto cam = curObj->GetCamera()) { int pt = 0; s >> cam->_fov >> cam->_near >> cam->_far >> pt; cam->_projType = (ProjectionType)pt; }
		}
		else if (tag == "cxf" && curObj) {
			Vec3 p, r; s >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z;
			if (auto t = curObj->GetTransform()) { t->SetLocalRotation(r); t->SetLocalPosition(p); }
			int act = 1; if (s >> act) curObj->SetActive(act != 0);
		}
		else if (tag == "cpar" && curObj) {
			std::string pn; std::getline(s >> std::ws, pn);
			if (pn != "-" && !pn.empty()) parentLinks.push_back({ curName, Utf8ToW(pn) });
		}
		// ── 터레인 ──
		else if (tag == "tobj") {
			std::string nm; std::getline(s >> std::ws, nm); curTerrainName = Utf8ToW(nm);
			curTerrain = findByName(curTerrainName);
			if (!curTerrain) {
				curTerrain = make_shared<GameObject>(); curTerrain->SetObjectName(curTerrainName); curTerrain->GetOrAddTransform();
				auto mr = make_shared<MeshRenderer>(); mr->Bind(this); curTerrain->AddComponent(mr);
				auto tr = make_shared<Terrain>(); tr->Bind(this); curTerrain->AddComponent(tr);
				_gameScene->Add(curTerrain);
			}
			curAny = curTerrain;
		}
		else if (tag == "tprm" && curTerrain) {
			int gn = 128; float cs = 1.f; s >> gn >> cs;
			if (auto tr = curTerrain->GetComponent<Terrain>()) tr->Init(gn, cs); // 평지 초기화(thm 가 덮어씀)
		}
		else if (tag == "thm" && curTerrain) {
			std::string hp; std::getline(s >> std::ws, hp);
			if (auto tr = curTerrain->GetComponent<Terrain>(); tr && !hp.empty()) tr->LoadHeightmap(Utf8ToW(hp));
		}
		else if (tag == "txf" && curTerrain) {
			Vec3 p; s >> p.x >> p.y >> p.z; if (auto t = curTerrain->GetTransform()) t->SetLocalPosition(p);
			int act = 1; if (s >> act) curTerrain->SetActive(act != 0);
		}
		// ── 식생 ──
		else if (tag == "fobj") {
			std::string nm; std::getline(s >> std::ws, nm); curFoliageName = Utf8ToW(nm);
			curFoliage = findByName(curFoliageName);
			if (!curFoliage) {
				curFoliage = make_shared<GameObject>(); curFoliage->SetObjectName(curFoliageName); curFoliage->GetOrAddTransform();
				auto fol = make_shared<Foliage>(); fol->Bind(this); curFoliage->AddComponent(fol);
				_gameScene->Add(curFoliage);
			}
			curFoliageOwner.clear();
		}
		else if (tag == "fown" && curFoliage) {
			std::string on; std::getline(s >> std::ws, on);
			if (on != "-" && !on.empty()) curFoliageOwner = Utf8ToW(on);
		}
		else if (tag == "fprm" && curFoliage) {
			int gc = 0, tc = 0; float gs = 0.4f; unsigned sd = 1337; s >> gc >> tc >> gs >> sd;
			auto fol = std::dynamic_pointer_cast<Foliage>(curFoliage->GetRenderer());
			auto ownerObj = curFoliageOwner.empty() ? nullptr : findByName(curFoliageOwner);
			auto terr = ownerObj ? ownerObj->GetTerrain() : nullptr;
			if (fol && terr) fol->Generate(terr.get(), gc, tc, gs, (uint32)sd); // 결정적 재생성
		}
		// ── 스크립트(MonoBehaviour) — 직전 블록 오브젝트에 부착 ──
		else if (tag == "mb" && curAny) {
			std::string name; s >> name;
			if (auto mb = ScriptRegistry::Create(name)) { std::string rest; std::getline(s, rest); std::istringstream is(rest); mb->Deserialize(is); curAny->AddComponent(mb); }
		}
	}
	// 부모 링크 (모든 오브젝트 생성 후 — LOCAL 유지)
	for (auto& link : parentLinks)
	{
		auto child = findByName(link.first), parent = findByName(link.second);
		if (child && parent)
			if (auto ct = child->GetTransform(), pt = parent->GetTransform(); ct && pt)
				ct->SetParentKeepLocal(pt);
	}

	if (!modelUtf8.empty())
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, modelUtf8.c_str(), (int)modelUtf8.size(), nullptr, 0);
		std::wstring wp(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, modelUtf8.c_str(), (int)modelUtf8.size(), wp.data(), n);
		_pendingModel = wp; // 다음 프레임 GPU 유휴 시 _scene 재로드(바닥/경로 갱신)
	}
	Log("Scene loaded: " + WToUtf8(path));
}

// 씬뷰 클릭 레이 → 모델 AABB / 바닥 평면 픽킹 → 하이어라키 선택
