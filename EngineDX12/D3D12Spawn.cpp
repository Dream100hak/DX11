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

// 오브젝트 라이프사이클 — Spawn/Duplicate/Delete/New/Play (D3D12Editor.cpp 에서 분리)
shared_ptr<GameObject> D3D12Device::SpawnMeshObject(const std::wstring& name, const vector<Vtx>& v, const vector<uint32>& idx, const Vec3& pos, MeshPrim prim, bool autoName)
{
	if (!_gameScene) return nullptr;
	auto obj = make_shared<GameObject>();
	obj->SetObjectName(autoName ? (name + L"_" + std::to_wstring(++_spawnCounter)) : name);
	auto tr = obj->GetOrAddTransform(); tr->SetLocalPosition(pos);
	auto mr = make_shared<MeshRenderer>(); mr->Bind(this); mr->SetGeometry(v, idx); mr->SetPrim(prim);
	obj->AddComponent(mr);
	_gameScene->Add(obj);
	if (autoName) { _selectedGO = obj; _sel = SelEntity::Model; Log("Created: " + WToUtf8(obj->GetObjectName())); }
	return obj;
}

// 프리미티브 종류 → 지오메트리 생성 (재생성/스폰 공용)
shared_ptr<GameObject> D3D12Device::SpawnLight(int type, const std::wstring& name, const Vec3& pos)
{
	if (!_gameScene) return nullptr;
	auto obj = make_shared<GameObject>();
	obj->SetObjectName(name + L"_" + std::to_wstring(++_spawnCounter));
	obj->GetOrAddTransform()->SetLocalPosition(pos);
	auto l = make_shared<Light>();
	l->_lightType = (LightType)type;
	if (type == 0)      { l->_color = Vec3{ 1.f, 0.96f, 0.88f }; l->_intensity = 1.2f; l->_direction = Vec3{ 0.3f, -1.f, 0.2f }; }
	else if (type == 1) { l->_color = Vec3{ 1.f, 0.8f, 0.6f };   l->_intensity = 3.f;  l->_range = 6.f; }
	else                { l->_color = Vec3{ 0.6f, 0.8f, 1.f };   l->_intensity = 5.f;  l->_range = 9.f; l->_spotAngleDeg = 28.f; l->_direction = Vec3{ 0.f, -1.f, 0.f }; }
	obj->AddComponent(l);
	_gameScene->Add(obj);
	_selectedGO = obj; _sel = SelEntity::Model;
	Log("Created light: " + WToUtf8(obj->GetObjectName()));
	return obj;
}

Vec3 D3D12Device::SpawnPoint()
{
	using namespace DirectX;
	XMVECTOR p = XMVectorAdd(_camera.Eye(), XMVectorScale(_camera.Forward(), 4.0f));
	Vec3 r; XMStoreFloat3(&r, p); r.y = max(r.y, 0.5f); // 바닥 아래로 안 가게
	return r;
}

shared_ptr<GameObject> D3D12Device::SpawnAnimatedModel(const std::wstring& meshPath, const Vec3& pos)
{
	if (!_gameScene) return nullptr;
	auto obj = make_shared<GameObject>();
	fs::path mp(meshPath);
	obj->SetObjectName(mp.stem().wstring() + L"_" + std::to_wstring(++_spawnCounter));
	obj->GetOrAddTransform()->SetLocalPosition(pos);
	auto an = make_shared<ModelAnimator>(); an->Bind(this);
	if (!an->Load(meshPath)) { Log("Animated model load FAILED: " + WToUtf8(meshPath)); return nullptr; }
	obj->AddComponent(an);
	_gameScene->Add(obj);
	_selectedGO = obj; _sel = SelEntity::Model;
	Log("Created animated: " + WToUtf8(obj->GetObjectName()));
	return obj;
}

// File > Convert FBX... — 파일 다이얼로그로 FBX 선택 → ufbx 변환(.mesh/.clip/.mat) → Models/<폴더>/ 에 저장 → 스폰
void D3D12Device::ConvertFbxDialog()
{
	wchar_t file[MAX_PATH] = L"";
	OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
	ofn.lpstrFilter = L"FBX Files\0*.fbx\0All Files\0*.*\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"Convert FBX"; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetOpenFileNameW(&ofn)) return; // 취소

	fs::path fbx(file);
	std::wstring stem = fbx.stem().wstring();
	// 출력 폴더: Assets\Models\<FBX 부모 폴더명>\  (DX11 컨버터와 동일 규칙)
	std::wstring parentName = fbx.has_parent_path() ? fbx.parent_path().filename().wstring() : L"Imported";
	if (parentName.empty()) parentName = L"Imported";
	std::wstring outDir = _assetRoot + L"\\Models\\" + parentName + L"\\";

	Log("Converting FBX: " + WToUtf8(fbx.wstring()));
	FbxConvertResult r = ConvertFbxToMesh(fbx.wstring(), outDir, stem);
	if (!r.ok) { Log("FBX convert FAILED: " + r.error); return; }
	Log("FBX converted: " + std::to_string(r.meshCount) + " mesh / " + std::to_string(r.boneCount) + " bone / "
		+ std::to_string(r.materialCount) + " mat / " + std::to_string(r.animCount) + " anim ("
		+ std::to_string(r.frameCount) + " frames) → " + WToUtf8(outDir));

	// 변환 결과 스폰 (ModelAnimator — 애니 없으면 바인드포즈 정적 메시로 렌더)
	Vec3 sp = SpawnPoint();
	SpawnAnimatedModel(r.meshPath, Vec3{ sp.x, 0, sp.z });
}

// 데모 씬 일괄 생성 — 터레인(언덕)+물+식생+불 파티클+색색 라이트. (전체 기능 코존재 검증/데모용)
void D3D12Device::SpawnShowcase()
{
	if (!_gameScene) return;
	auto terrObj = SpawnTerrain(96, 1.0f);
	if (terrObj) if (auto tr = terrObj->GetComponent<Terrain>())
	{
		for (int i = 0; i < 24; ++i) tr->Sculpt(-14, -10, 12.f, 0.5f, TerrainBrush::Raise, 0); // 언덕1
		for (int i = 0; i < 16; ++i) tr->Sculpt(16, 12, 9.f, 0.5f, TerrainBrush::Raise, 0);     // 언덕2
		_folGrass = 5000; _folTree = 50; _folSize = 0.4f;
		GenerateFoliage(terrObj);
	}
	_terrainEdit = false;
	_waterOn = true; _waterLevel = 0.4f; _tessTerrain = true; _tessFactor = 20.f;

	// 불 파티클
	{ auto o = SpawnEmpty(L"Fire", Vec3{ 0, 0.5f, 0 }); if (o) { auto ps = make_shared<ParticleSystem>(); ps->_mode = 2; ps->_rate = 180.f; ps->_size = 0.18f; ps->_sizeEnd = 0.02f; ps->_speed = 2.4f; o->AddComponent(ps); } }
	// 색색 점광원
	const Vec3 cols[4] = { {1,0.4f,0.2f},{0.3f,0.6f,1},{0.4f,1,0.5f},{1,0.3f,0.8f} };
	for (int i = 0; i < 4; ++i) { float a = i * 1.5708f; auto o = SpawnLight(1, L"ShowLight", Vec3{ cosf(a) * 6.f, 2.5f, sinf(a) * 6.f }); if (o) if (auto l = o->GetLight()) { l->_color = cols[i]; l->_intensity = 3.f; l->_range = 7.f; } }

	_camera.pos = { 30, 22, -30 }; _camera.yaw = -0.78f; _camera.pitch = -0.45f;
	Log("Showcase scene spawned (terrain+water+foliage+fire+lights)");
}

// 캐릭터/프롭 쇼케이스 — 변환된 .mesh 모델들을 배치해 PBR/IBL/애니/TAA/파티클을 한눈에 체감.
void D3D12Device::SpawnCharacterShowcase()
{
	if (!_gameScene) return;
	auto modelPath = [&](const wchar_t* rel) { return _assetRoot + L"\\Models\\" + rel; };

	// 애니 캐릭터 3종 (Archer 는 기본 씬에 이미 존재 → 함께 줄세움). 각자 기본 클립 자동 재생.
	struct Spawn { const wchar_t* rel; Vec3 pos; };
	const Spawn chars[] = {
		{ L"Kachujin\\Kachujin.mesh", Vec3{ -3.f, 0, 0 } },
		{ L"Mutant\\Mutant.mesh",     Vec3{  3.f, 0, 0 } },
		{ L"Enemy\\Enemy.mesh",       Vec3{  0.f, 0, 3.f } },
	};
	for (auto& s : chars) SpawnAnimatedModel(modelPath(s.rel), s.pos);

	// (Tower2 프롭은 .mesh 포맷이 스키닝 로더와 비호환 — 재변환 후 추가 예정)

	// 분위기 FX — 불 파티클 (가산 글로우)
	if (auto o = SpawnEmpty(L"FX_Fire", Vec3{ 0.f, 0.3f, -2.5f }))
	{
		auto ps = make_shared<ParticleSystem>();
		ps->_shape = ParticleSystem::ShCone; ps->_coneAngle = 18.f; ps->_blend = ParticleSystem::BlendAdd;
		ps->_rate = 130.f; ps->_life = 1.1f; ps->_speed = 1.6f; ps->_gravity = 1.2f;
		ps->_size = 0.18f; ps->_sizeEnd = 0.02f; ps->_soft = 0.8f; ps->_fadeIn = 0.1f;
		ps->_color = { 1.f, 0.7f, 0.2f }; ps->_colorEnd = { 1.f, 0.15f, 0.03f };
		o->AddComponent(ps);
	}

	// 품질 옵션 보장 (체감용) + 카메라 프레이밍
	_iblOn = true; _taaOn = true; _bloomOn = true;
	// 정면(+z) 약간 위에서 그룹 프레이밍 (yaw=0 → +z 시선)
	_camera.pos = { 0.f, 2.6f, -8.0f }; _camera.yaw = 0.f; _camera.pitch = -0.14f; _camera.orbit = false;
	Log("Character showcase spawned (Archer/Kachujin/Mutant/Enemy + fire)");
}

// 선택 GameObject → .prefab (Mesh/Animator). 텍스트 포맷: type/prim|mesh/mat/xform.
void D3D12Device::SaveSelectedAsPrefab()
{
	auto go = _selectedGO; if (!go) { Log("Prefab: no selection"); return; }
	auto t = go->GetTransform(); if (!t) return;
	wchar_t file[MAX_PATH] = L"prefab.prefab";
	OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
	ofn.lpstrFilter = L"Prefab (*.prefab)\0*.prefab\0"; ofn.lpstrDefExt = L"prefab";
	ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	std::wstring dir = _assetRoot + L"\\Prefabs"; std::error_code ec; fs::create_directories(dir, ec); ofn.lpstrInitialDir = dir.c_str();
	if (!GetSaveFileNameW(&ofn)) return;

	std::ofstream f(file); if (!f) { Log("Prefab save FAILED"); return; }
	f << "name " << WToUtf8(go->GetObjectName()) << '\n';
	if (auto an = go->GetModelAnimator())
	{
		f << "type anim\n";
		f << "mesh " << WToUtf8(an->MeshDir() + an->MeshStem() + L".mesh") << '\n';
		f << "clip " << an->GetClipIndex() << ' ' << an->GetSpeed() << ' ' << (an->IsPlaying() ? 1 : 0) << '\n';
	}
	else if (auto mr = go->GetMeshRenderer())
	{
		f << "type mesh\n";
		f << "prim " << (int)mr->GetPrim() << '\n';
		Material& m = mr->GetMaterial();
		if (!m._path.empty()) f << "matref " << WToUtf8(m._path) << '\n';
		else f << "mat " << m._diffuse.x << ' ' << m._diffuse.y << ' ' << m._diffuse.z << ' ' << m._metallic << ' ' << m._roughness << ' ' << m._emissive
		       << ' ' << (m._diffuseTex.empty() ? std::string("-") : WToUtf8(m._diffuseTex)) << '\n';
	}
	else { Log("Prefab: only Mesh/Animator supported"); return; }
	Vec3 p = t->GetLocalPosition(), r = t->GetLocalRotation(), s = t->GetLocalScale();
	f << "xform " << p.x << ' ' << p.y << ' ' << p.z << ' ' << r.x << ' ' << r.y << ' ' << r.z << ' ' << s.x << ' ' << s.y << ' ' << s.z << '\n';
	Log("Prefab saved: " + WToUtf8(file));
}

// .prefab → 씬에 새 인스턴스 스폰 (카메라 앞).
void D3D12Device::InstantiatePrefab()
{
	wchar_t file[MAX_PATH] = L"";
	OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
	ofn.lpstrFilter = L"Prefab (*.prefab)\0*.prefab\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	std::wstring dir = _assetRoot + L"\\Prefabs"; ofn.lpstrInitialDir = dir.c_str();
	if (!GetOpenFileNameW(&ofn)) return;

	std::ifstream f(file); if (!f) { Log("Prefab open FAILED"); return; }
	std::string line, type, meshPath, matTex; int prim = 1, clipIdx = 0, playing = 1;
	float spd = 1.f; Vec3 diff{ 1,1,1 }; float met = 0, rough = 0.5f, emis = 0; std::wstring matRef;
	Vec3 p{ 0,0,0 }, r{ 0,0,0 }, sc{ 1,1,1 }; bool hasXf = false;
	while (std::getline(f, line))
	{
		std::istringstream s(line); std::string tag; s >> tag;
		if (tag == "type") s >> type;
		else if (tag == "prim") s >> prim;
		else if (tag == "mesh") { std::getline(s >> std::ws, meshPath); }
		else if (tag == "clip") s >> clipIdx >> spd >> playing;
		else if (tag == "matref") { std::string mp; std::getline(s >> std::ws, mp); matRef = Utf8ToW(mp); }
		else if (tag == "mat") { s >> diff.x >> diff.y >> diff.z >> met >> rough >> emis; std::getline(s >> std::ws, matTex); }
		else if (tag == "xform") { s >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z >> sc.x >> sc.y >> sc.z; hasXf = true; }
	}
	Vec3 at = SpawnPoint();
	shared_ptr<GameObject> obj;
	if (type == "anim") obj = SpawnAnimatedModel(Utf8ToW(meshPath), at);
	else
	{
		vector<Vtx> v; vector<uint32> idx; MeshPrim mp = (MeshPrim)prim; BuildPrim(mp, v, idx);
		obj = SpawnMeshObject(L"Prefab", v, idx, at, mp);
		if (obj) if (auto mr = obj->GetMeshRenderer())
		{
			if (!matRef.empty()) { auto sh = GET_SINGLE(ResourceManager)->Get<Material>(matRef); if (!sh) { sh = LoadMaterial(matRef); if (sh) GET_SINGLE(ResourceManager)->Add<Material>(matRef, sh); } if (sh) mr->SetMaterialRef(sh); }
			else { Material& m = mr->GetMaterial(); m._diffuse = diff; m._metallic = met; m._roughness = rough; m._emissive = emis; if (matTex != "-" && !matTex.empty()) m._diffuseTex = Utf8ToW(matTex); }
		}
	}
	if (obj && hasXf) if (auto t = obj->GetTransform()) { t->SetLocalScale(sc); t->SetLocalRotation(r); /*위치는 SpawnPoint 유지*/ }
	if (obj && type == "anim") if (auto an = obj->GetModelAnimator()) { an->SetClipIndex(clipIdx); an->SetSpeed(spd); an->SetPlaying(playing != 0); }
	Log("Prefab instantiated: " + WToUtf8(file));
}

// Terrain GameObject — MeshRenderer(그리드 메시) + Terrain(하이트맵/스컬프트). 트랜스폼 항등(정점=월드).
shared_ptr<GameObject> D3D12Device::SpawnTerrain(int gridN, float cellSize)
{
	if (!_gameScene) return nullptr;
	auto obj = make_shared<GameObject>();
	obj->SetObjectName(L"Terrain_" + std::to_wstring(++_spawnCounter));
	obj->GetOrAddTransform();
	auto mr = make_shared<MeshRenderer>(); mr->Bind(this);
	obj->AddComponent(mr);
	auto tr = make_shared<Terrain>(); tr->Bind(this);
	obj->AddComponent(tr);
	tr->Init(gridN, cellSize);   // MeshRenderer 지오메트리 설정 (Add 후 — GetMeshRenderer 동작)
	_gameScene->Add(obj);
	_selectedGO = obj; _sel = SelEntity::Model;
	_terrainEdit = true;         // 생성 직후 편집 모드 진입
	Log("Created Terrain: " + std::to_string(gridN) + "x" + std::to_string(gridN) + " cells");
	return obj;
}

// 터레인용 식생 GameObject 생성/재생성 — Foliage 렌더러(터레인 표면에 잔디/나무 산포)
void D3D12Device::GenerateFoliage(const shared_ptr<GameObject>& terrainObj)
{
	if (!_gameScene || !terrainObj) return;
	auto terr = terrainObj->GetTerrain(); if (!terr) return;

	// 기존 식생 재사용 — 터레인의 자식 중 Foliage 렌더러를 가진 GameObject (이름 매칭 대신 부모-자식 링크: 리네임/세이브에 견고)
	shared_ptr<GameObject> folObj;
	shared_ptr<Foliage> fol;
	if (auto tt = terrainObj->GetTransform())
		for (auto& ct : tt->GetChildren())
			if (ct) if (auto cgo = ct->GetGameObject())
				if (auto f = std::dynamic_pointer_cast<Foliage>(cgo->GetRenderer())) { folObj = cgo; fol = f; break; }
	if (!folObj)
	{
		folObj = make_shared<GameObject>();
		folObj->SetObjectName(terrainObj->GetObjectName() + L"_Foliage");
		folObj->GetOrAddTransform();
		fol = make_shared<Foliage>(); fol->Bind(this);
		folObj->AddComponent(fol);
		_gameScene->Add(folObj);
		// 터레인 자식으로 연결 (Foliage 는 자체 트랜스폼 미사용 — 렌더링 영향 없음)
		if (auto ft = folObj->GetTransform(), tt = terrainObj->GetTransform(); ft && tt) ft->SetParentKeepWorld(tt);
	}
	if (!fol) return;
	fol->Generate(terr.get(), _folGrass, _folTree, _folSize, (uint32)_folSeed);
	Log("Foliage generated: " + std::to_string(_folGrass) + " grass, " + std::to_string(_folTree) + " trees");
}

// 카메라를 대상 지점으로 이동 + 시선 정렬 (Frame/Focus 공용)
void D3D12Device::FocusCameraOn(const Vec3& target)
{
	using namespace DirectX;
	_camera.pos = { target.x + 4.f, target.y + 3.f, target.z - 4.f };
	XMVECTOR dir = XMVector3Normalize(XMVectorSet(target.x - _camera.pos.x, target.y - _camera.pos.y, target.z - _camera.pos.z, 0));
	XMFLOAT3 d; XMStoreFloat3(&d, dir);
	_camera.yaw = atan2f(d.x, d.z);
	_camera.pitch = asinf(d.y);
}

// 모든 비-내부 오브젝트의 월드 위치 AABB 중심으로 카메라 프레이밍 (Home / "Frame All")
void D3D12Device::FrameAll()
{
	if (!_gameScene) return;
	using namespace DirectX;
	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f); bool any = false;
	for (auto& kv : _gameScene->GetCreatedObjects())
	{
		auto& o = kv.second; if (!o || o->IsEditorInternal() || !o->GetTransform()) continue;
		Matrix wm = o->GetTransform()->GetWorldMatrix();
		mn.x = min(mn.x, wm._41); mn.y = min(mn.y, wm._42); mn.z = min(mn.z, wm._43);
		mx.x = max(mx.x, wm._41); mx.y = max(mx.y, wm._42); mx.z = max(mx.z, wm._43); any = true;
	}
	if (!any) return;
	Vec3 c{ (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
	float ext = max(max(mx.x - mn.x, mx.y - mn.y), mx.z - mn.z) * 0.5f + 3.f;
	_camera.pos = { c.x + ext, c.y + ext * 0.7f, c.z - ext };
	XMVECTOR dir = XMVector3Normalize(XMVectorSet(c.x - _camera.pos.x, c.y - _camera.pos.y, c.z - _camera.pos.z, 0));
	XMFLOAT3 d; XMStoreFloat3(&d, dir); _camera.yaw = atan2f(d.x, d.z); _camera.pitch = asinf(d.y);
}

// 씬뷰 uv → 월드 레이 → 선택 Terrain 커서 갱신 + (apply 시) 스컬프트
void D3D12Device::TerrainBrushAt(float u, float v, bool apply)
{
	using namespace DirectX;
	if (!_selectedGO) { _terrainCursorValid = false; return; }
	auto tr = _selectedGO->GetComponent<Terrain>();
	if (!tr) { _terrainCursorValid = false; return; }

	float nx = u * 2.f - 1.f, ny = (1.f - v) * 2.f - 1.f;
	XMMATRIX invVP = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_viewM) * XMLoadFloat4x4(&_projM));
	XMVECTOR n = XMVector4Transform(XMVectorSet(nx, ny, 0, 1), invVP); n = XMVectorScale(n, 1.f / XMVectorGetW(n));
	XMVECTOR f = XMVector4Transform(XMVectorSet(nx, ny, 1, 1), invVP); f = XMVectorScale(f, 1.f / XMVectorGetW(f));
	Vec3 ro; XMStoreFloat3(&ro, n);
	Vec3 rd; XMStoreFloat3(&rd, XMVector3Normalize(XMVectorSubtract(f, n)));

	Vec3 hit;
	if (!tr->Raycast(ro, rd, hit)) { _terrainCursorValid = false; return; }
	_terrainCursor = hit; _terrainCursorValid = true;

	if (apply)
	{
		float dt = ImGui::GetIO().DeltaTime; if (dt <= 0.f || dt > 0.1f) dt = 1.f / 60.f;
		if (_terrainBrush == 4) // Paint
			tr->Paint(hit.x, hit.z, _terrainRadius, _terrainStrength * dt, _terrainPaintColor);
		else
		{
			float str = _terrainStrength * dt;
			if (_terrainBrush == 2 || _terrainBrush == 3) str = _terrainStrength * dt * 0.5f; // smooth/flatten 은 비율
			tr->Sculpt(hit.x, hit.z, _terrainRadius, str, (TerrainBrush)_terrainBrush, _terrainFlatten);
		}
	}
}

shared_ptr<GameObject> D3D12Device::SpawnEmpty(const std::wstring& name, const Vec3& pos)
{
	if (!_gameScene) return nullptr;
	auto obj = make_shared<GameObject>();
	obj->SetObjectName(name + L"_" + std::to_wstring(++_spawnCounter));
	obj->GetOrAddTransform()->SetLocalPosition(pos);
	_gameScene->Add(obj);
	_selectedGO = obj;
	Log("Created Empty: " + WToUtf8(obj->GetObjectName()));
	return obj;
}

// 부모에서 분리 + 자식 루트 승격(댕글링 방지) + 씬 제거
void D3D12Device::RemoveObject(const shared_ptr<GameObject>& obj)
{
	if (!obj || !_gameScene) return;
	if (auto t = obj->GetTransform())
	{
		auto kids = t->GetChildren(); // 복사 — SetParentKeepWorld 가 _children 변경
		for (auto& c : kids) if (c) c->SetParentKeepWorld(nullptr);
		if (auto p = t->GetParent()) p->RemoveChild(t.get());
		t->SetParent(nullptr);
	}
	_gameScene->Remove(obj);
}

void D3D12Device::DeleteSelectedObject()
{
	if (!_gameScene) return;
	// 멀티셀렉트 추가분 먼저 제거
	for (int64 id : _selIds)
	{
		auto o = _gameScene->GetCreatedObject(id);
		if (o && !o->IsEditorInternal() && o != _modelObj) { Log("Deleted: " + WToUtf8(o->GetObjectName())); RemoveObject(o); }
	}
	_selIds.clear();
	if (!_selectedGO) return;
	if (_selectedGO->IsEditorInternal()) { Log("Cannot delete editor-internal object"); return; }
	if (_selectedGO == _modelObj) { Log("Cannot delete the main Model"); return; }
	Log("Deleted: " + WToUtf8(_selectedGO->GetObjectName()));
	RemoveObject(_selectedGO);
	_selectedGO = nullptr;
}

// 스폰/추가 오브젝트(메시/애니/라이트) 전부 제거 — 모델/고정라이트/카메라/내부 유지
int D3D12Device::ClearDynamicObjects()
{
	if (!_gameScene) return 0;
	std::vector<shared_ptr<GameObject>> toRemove;
	for (auto& kv : _gameScene->GetCreatedObjects())
	{
		auto& o = kv.second;
		if (!o || o->IsEditorInternal() || o == _modelObj) continue;
		if (o == _sunObj || o == _ptObj || o == _spotObj) continue; // 고정 라이트 유지
		if (o->GetRenderer() || o->GetLight()) toRemove.push_back(o);
	}
	for (auto& o : toRemove) RemoveObject(o);
	_selectedGO = nullptr;
	return (int)toRemove.size();
}

// 새 씬 — 동적 오브젝트 제거 + 파라미터 리셋
void D3D12Device::NewScene()
{
	int n = ClearDynamicObjects();
	Log("New Scene: removed " + std::to_string(n) + " object(s)");
	_sel = SelEntity::Model;
	_spawnCounter = 0;
	ResetDefaults(); // 포스트/라이팅 파라미터 + 모델 트랜스폼 리셋
}

// 선택(primary + 멀티셀렉트) 을 무게중심에 생성한 빈 GameObject 아래로 그룹화 (Ctrl+G)
void D3D12Device::GroupSelected()
{
	if (!_gameScene || !_selectedGO || _selectedGO->IsEditorInternal()) return;
	// 대상 수집 (primary + _selIds, 에디터 내부 제외)
	std::vector<shared_ptr<GameObject>> targets;
	if (_selectedGO) targets.push_back(_selectedGO);
	for (int64 id : _selIds)
		if (auto o = _gameScene->GetCreatedObject(id); o && !o->IsEditorInternal()) targets.push_back(o);
	if (targets.empty()) return;
	// 무게중심(월드)
	using namespace DirectX;
	XMFLOAT3 c{ 0,0,0 }; int cnt = 0;
	for (auto& o : targets) if (auto t = o->GetTransform()) { XMFLOAT4X4 wm = t->GetWorldMatrix(); c.x += wm._41; c.y += wm._42; c.z += wm._43; ++cnt; }
	if (cnt > 0) { c.x /= cnt; c.y /= cnt; c.z /= cnt; }
	auto group = SpawnEmpty(L"Group", Vec3{ c.x, c.y, c.z });
	if (!group) return;
	auto gt = group->GetTransform();
	for (auto& o : targets)
		if (auto ot = o->GetTransform(); ot && gt) ot->SetParentKeepWorld(gt); // 월드 유지 재부모
	_selIds.clear(); _selectedGO = group; _anchorId = group->GetId();
	Log("Grouped " + std::to_string(cnt) + " object(s)");
}

// 선택(primary + 멀티) 위치를 이동 스냅(_snapT) 격자에 반올림 정렬
void D3D12Device::SnapSelectedToGrid()
{
	if (!_gameScene) return;
	float g = _snapT > 0.001f ? _snapT : 0.5f;
	auto snap = [&](const shared_ptr<GameObject>& o)
	{
		if (!o || o->IsEditorInternal()) return;
		if (auto t = o->GetTransform())
		{
			Vec3 p = t->GetLocalPosition();
			p.x = roundf(p.x / g) * g; p.y = roundf(p.y / g) * g; p.z = roundf(p.z / g) * g;
			t->SetLocalPosition(p);
		}
	};
	snap(_selectedGO);
	for (int64 id : _selIds) snap(_gameScene->GetCreatedObject(id));
}

// Play=현재 씬 스냅샷 저장 / Stop=스냅샷 복원 (플레이 중 편집 롤백, Unity 식)
void D3D12Device::TogglePlay()
{
	std::wstring snap = (std::filesystem::path(_assetRoot) / L"Scenes" / L"__play_snapshot.rtscene").wstring();
	if (!_playing)
	{
		_playCamPos = _camera.pos; _playCamYaw = _camera.yaw; _playCamPitch = _camera.pitch; // 에디터 카메라 포즈 캡처
		std::error_code ec; std::filesystem::create_directories(std::filesystem::path(snap).parent_path(), ec);
		SaveSceneTo(snap);
		_playing = true;
		Log("▶ Play (snapshot saved)");
	}
	else
	{
		_playing = false;
		ClearDynamicObjects();           // 플레이 중 스폰된 것 제거
		LoadSceneFrom(snap);             // 스냅샷 복원
		_camera.pos = _playCamPos; _camera.yaw = _playCamYaw; _camera.pitch = _playCamPitch; // 에디터 카메라 플레이 전으로 복원
		Log("■ Stop (snapshot restored)");
	}
}

// 멀티셀렉트 포함 그룹 복제
void D3D12Device::DuplicateSelectedObject()
{
	if (!_selectedGO || !_gameScene) return;
	std::vector<shared_ptr<GameObject>> srcs;
	srcs.push_back(_selectedGO);
	for (int64 id : _selIds) if (auto o = _gameScene->GetCreatedObject(id)) srcs.push_back(o);
	_selIds.clear();
	for (auto& s : srcs) DuplicateObject(s);
}

void D3D12Device::DuplicateObject(const shared_ptr<GameObject>& source)
{
	if (!source || !_gameScene) return;

	// 터레인 복제 — 일반 메시로 복제하면 사본에 Terrain 컴포넌트가 없어 TLAS OOB(동결) → 반드시 Terrain 으로 복제
	if (auto sterr = source->GetComponent<Terrain>())
	{
		auto obj = SpawnTerrain(sterr->GridN(), sterr->CellSize());
		if (obj) if (auto dterr = obj->GetComponent<Terrain>())
		{
			dterr->CopyFrom(*sterr);
			if (auto st = source->GetTransform(), dt = obj->GetTransform(); st && dt)
			{ Vec3 p = st->GetLocalPosition(); p.x += 2.0f; dt->SetLocalPosition(p); }
		}
		return;
	}

	// 애니메이션 모델 복제
	if (auto sa = source->GetModelAnimator())
	{
		auto st = source->GetTransform();
		Vec3 pos{ 0,0,0 }; if (st) { pos = st->GetLocalPosition(); pos.x += 1.0f; }
		auto obj = SpawnAnimatedModel(sa->MeshDir() + sa->MeshStem() + L".mesh", pos);
		if (obj)
		{
			if (auto da = obj->GetModelAnimator()) { da->SetClipIndex(sa->GetClipIndex()); da->SetSpeed(sa->GetSpeed()); da->SetPlaying(sa->IsPlaying()); }
			if (auto dt = obj->GetTransform(); st && dt) { dt->SetLocalScale(st->GetLocalScale()); dt->SetLocalRotation(st->GetLocalRotation()); }
		}
		return;
	}

	auto src = source->GetMeshRenderer();
	if (!src) { Log("Duplicate: only Mesh/Animator objects supported"); return; }
	auto st = source->GetTransform();
	Vec3 pos{ 0,0,0 };
	if (st) { pos = st->GetLocalPosition(); pos.x += 1.0f; }
	auto obj = SpawnMeshObject(source->GetObjectName(), src->GetLocalVerts(), src->GetLocalIndices(), pos, src->GetPrim()); // prim 종류 복사(직렬화 복원용)
	if (obj) // 트랜스폼(회전/스케일) + 머티리얼 복사
	{
		if (auto dt = obj->GetTransform(); st && dt)
		{ dt->SetLocalScale(st->GetLocalScale()); dt->SetLocalRotation(st->GetLocalRotation()); }
		if (auto dr = obj->GetMeshRenderer())
		{
			if (!src->GetMaterialRef()->_path.empty()) dr->SetMaterialRef(src->GetMaterialRef()); // 공유 자산 → 참조 공유
			else dr->GetMaterial() = src->GetMaterial();                                          // 인라인 → 값 복사
		}
	}
}

// ── 씬 저장/로드 (.rtscene 텍스트) — <Assets>/Scenes/quick.rtscene ──
