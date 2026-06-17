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
#include <functional>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

namespace fs = std::filesystem;

// wstring → UTF-8 (ImGui 텍스트용)
static std::string WToUtf8(const std::wstring& w)
{
	if (w.empty()) return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}
// Utf8ToW 는 MeshLoader.h 에 정의됨 (재사용)
static void BuildPrim(MeshPrim prim, vector<Vtx>& v, vector<uint32>& idx); // 프리미티브 지오메트리 생성

// W1 파티클 CPU 시뮬레이션 (sparks 분수 / snow 낙하)
void D3D12Device::UpdateParticles(float dt)
{
	if (!_particlesOn) { _particles.clear(); return; }
	auto rnd = [](float& s) { s = fmodf(s * 1664525.0f + 1013904223.0f, 4294967296.0f); return s / 4294967296.0f; };
	const int CAP = 240;
	static float seed = 12345.0f;
	while ((int)_particles.size() < CAP)
	{
		Particle p;
		if (_particleMode == 0) // sparks 분수
		{
			p.pos = { 0, 0.1f, 0 };
			p.vel = { (rnd(seed) - 0.5f) * 2.2f, 2.5f + rnd(seed) * 2.5f, (rnd(seed) - 0.5f) * 2.2f };
			p.col = { 1.4f, 0.5f + rnd(seed) * 0.4f, 0.12f }; p.life = 0.8f + rnd(seed) * 0.9f;
		}
		else // snow 낙하
		{
			p.pos = { (rnd(seed) - 0.5f) * 10.0f, 5.0f, (rnd(seed) - 0.5f) * 10.0f };
			p.vel = { (rnd(seed) - 0.5f) * 0.3f, -0.8f - rnd(seed) * 0.6f, (rnd(seed) - 0.5f) * 0.3f };
			p.col = { 0.9f, 0.95f, 1.1f }; p.life = 4.0f + rnd(seed) * 3.0f;
		}
		_particles.push_back(p);
	}
	for (auto it = _particles.begin(); it != _particles.end(); )
	{
		it->life -= dt;
		if (_particleMode == 0) it->vel.y -= 6.0f * dt; // 중력
		it->pos.x += it->vel.x * dt; it->pos.y += it->vel.y * dt; it->pos.z += it->vel.z * dt;
		bool dead = it->life <= 0.0f || (_particleMode == 0 && it->pos.y < 0.0f) || (_particleMode == 1 && it->pos.y < 0.0f);
		if (dead) it = _particles.erase(it); else ++it;
	}
}

// 디버그 라인 빌드 + 드로우 (U10 본 / U11 AABB / U12 라이트 아이콘 / U13 스팟 콘 / W1 파티클)
// 라인 GPU 플러밍은 _debugDraw(DebugDraw) 가 담당 — 여기선 에디터 상태를 읽어 라인을 채운다.
void D3D12Device::DrawDebugLines()
{
	using namespace DirectX;
	_debugDraw.Begin();

	if (_particlesOn) // W1: 파티클을 작은 월드 크로스로
		for (auto& pt : _particles) { float s = (_particleMode == 0) ? 0.05f : 0.03f; _debugDraw.Cross(pt.pos, pt.col, s); }

	if (_showAABB && _sel == SelEntity::Model)
	{
		XMFLOAT3 n = _scene._modelMin, x = _scene._modelMax, c{ 1.0f, 0.6f, 0.1f };
		XMFLOAT3 p[8] = { {n.x,n.y,n.z},{x.x,n.y,n.z},{x.x,n.y,x.z},{n.x,n.y,x.z},{n.x,x.y,n.z},{x.x,x.y,n.z},{x.x,x.y,x.z},{n.x,x.y,x.z} };
		int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
		for (int k = 0; k < 24; k += 2) _debugDraw.Line(p[e[k]], p[e[k + 1]], c);
	}
	// ── 씬 Light 컴포넌트 아이콘 (고정 3개 + 추가 라이트 전부) ──
	if (_showLightIcons && _gameScene)
	{
		auto wireCircle = [&](XMVECTOR c, XMVECTOR a1, XMVECTOR a2, float r, XMFLOAT3 col)
		{
			XMFLOAT3 prev{};
			for (int s = 0; s <= 16; ++s)
			{
				float a = s / 16.0f * 6.2831853f;
				XMVECTOR pw = XMVectorAdd(c, XMVectorAdd(XMVectorScale(a1, cosf(a) * r), XMVectorScale(a2, sinf(a) * r)));
				XMFLOAT3 p; XMStoreFloat3(&p, pw); if (s > 0) _debugDraw.Line(prev, p, col); prev = p;
			}
		};
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive()) continue;
			auto l = o->GetLight(); if (!l || !l->_enabled || l->_lightType == LightType::Directional) continue; // 디렉셔널=글로벌(생략)
			auto t = o->GetTransform(); Vec3 pp = t ? t->GetLocalPosition() : Vec3{ 0,0,0 };
			XMFLOAT3 col = l->_color;
			_debugDraw.Cross(pp, col, 0.2f);
			XMVECTOR c = XMLoadFloat3(&pp);
			wireCircle(c, XMVectorSet(1,0,0,0), XMVectorSet(0,1,0,0), 0.3f, col); // 전구 와이어 구(3축 원)
			wireCircle(c, XMVectorSet(1,0,0,0), XMVectorSet(0,0,1,0), 0.3f, col);
			wireCircle(c, XMVectorSet(0,1,0,0), XMVectorSet(0,0,1,0), 0.3f, col);
			if (l->_lightType == LightType::Spot && _showSpotCone)
			{
				XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&l->_direction));
				XMVECTOR up = fabsf(XMVectorGetY(dir)) > 0.95f ? XMVectorSet(1,0,0,0) : XMVectorSet(0,1,0,0);
				XMVECTOR rt = XMVector3Normalize(XMVector3Cross(up, dir)); XMVECTOR bt = XMVector3Cross(dir, rt);
				float len = min(l->_range, 5.0f), rad = tanf(l->_spotAngleDeg * 0.01745f) * len;
				XMVECTOR ce = XMVectorAdd(c, XMVectorScale(dir, len));
				XMFLOAT3 prev{};
				for (int s = 0; s <= 16; ++s)
				{
					float a = s / 16.0f * 6.2831853f;
					XMVECTOR pw = XMVectorAdd(ce, XMVectorAdd(XMVectorScale(rt, cosf(a) * rad), XMVectorScale(bt, sinf(a) * rad)));
					XMFLOAT3 p; XMStoreFloat3(&p, pw);
					if (s > 0) _debugDraw.Line(prev, p, col);
					if (s % 4 == 0) _debugDraw.Line(pp, p, col);
					prev = p;
				}
			}
		}
	}

	// ── 콜라이더 와이어프레임 (초록) ──
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive()) continue;
			auto col = o->GetComponent<BaseCollider>(); if (!col) continue;
			const XMFLOAT3 gc{ 0.2f, 1.0f, 0.3f };
			if (auto box = std::dynamic_pointer_cast<AABBBoxCollider>(col))
			{
				Vec3 n, x; box->WorldBounds(n, x);
				XMFLOAT3 p[8] = { {n.x,n.y,n.z},{x.x,n.y,n.z},{x.x,n.y,x.z},{n.x,n.y,x.z},{n.x,x.y,n.z},{x.x,x.y,n.z},{x.x,x.y,x.z},{n.x,x.y,x.z} };
				int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
				for (int k = 0; k < 24; k += 2) _debugDraw.Line(p[e[k]], p[e[k + 1]], gc);
			}
			else if (auto sph = std::dynamic_pointer_cast<SphereCollider>(col))
			{
				XMVECTOR c = XMVectorSet(0, 0, 0, 1);
				if (auto t = o->GetTransform()) { Vec3 cp = t->GetLocalPosition(); c = XMVectorSet(cp.x + sph->_center.x, cp.y + sph->_center.y, cp.z + sph->_center.z, 1); }
				float r = sph->WorldRadius();
				auto circ = [&](XMVECTOR a1, XMVECTOR a2) { XMFLOAT3 prev{};
					for (int s = 0; s <= 20; ++s) { float a = s / 20.f * 6.2831853f;
						XMVECTOR pw = XMVectorAdd(c, XMVectorAdd(XMVectorScale(a1, cosf(a) * r), XMVectorScale(a2, sinf(a) * r)));
						XMFLOAT3 pp; XMStoreFloat3(&pp, pw); if (s > 0) _debugDraw.Line(prev, pp, gc); prev = pp; } };
				circ(XMVectorSet(1,0,0,0), XMVectorSet(0,1,0,0)); circ(XMVectorSet(1,0,0,0), XMVectorSet(0,0,1,0)); circ(XMVectorSet(0,1,0,0), XMVectorSet(0,0,1,0));
			}
		}

	// ── ParticleSystem 컴포넌트 입자 (크로스) ──
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive()) continue;
			auto ps = std::dynamic_pointer_cast<ParticleSystem>(o->GetRenderer()); if (!ps) continue;
			float sz = ps->Size();
			for (auto& p : ps->Particles())
			{
				float fade = p.maxLife > 0 ? p.life / p.maxLife : 1.f;
				_debugDraw.Cross(p.pos, XMFLOAT3(p.col.x * fade, p.col.y * fade, p.col.z * fade), sz);
			}
		}

	// ── 게임 카메라 프러스텀 (노랑) ──
	if (_gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive() || o->IsEditorInternal()) continue;
			auto cam = o->GetCamera(); auto t = o->GetTransform(); if (!cam || !t) continue;
			Matrix wm = t->GetWorldMatrix(); XMMATRIX W = XMLoadFloat4x4(&wm);
			float nz = cam->_near, fz = min(cam->_far, 12.0f); // 멀리 잘라 화면에 들어오게
			float aspect = float(_gameW) / float(_gameH);
			float th = tanf(XMConvertToRadians(cam->_fov) * 0.5f);
			auto corner = [&](float z, float sx, float sy) {
				float h = th * z, w = h * aspect;
				XMVECTOR lp = XMVectorSet(sx * w, sy * h, z, 1);
				XMFLOAT3 p; XMStoreFloat3(&p, XMVector3TransformCoord(lp, W)); return p; };
			XMFLOAT3 c[8] = { corner(nz,-1,-1), corner(nz,1,-1), corner(nz,1,1), corner(nz,-1,1),
							  corner(fz,-1,-1), corner(fz,1,-1), corner(fz,1,1), corner(fz,-1,1) };
			int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
			const XMFLOAT3 yc{ 1.0f, 0.9f, 0.2f };
			for (int k = 0; k < 24; k += 2) _debugDraw.Line(c[e[k]], c[e[k + 1]], yc);
		}

	// ── 터레인 브러시 링 (편집 모드 + 유효 커서) ──
	if (_terrainEdit && _terrainCursorValid && _selectedGO && _selectedGO->GetComponent<Terrain>())
	{
		auto terr = _selectedGO->GetComponent<Terrain>();
		XMFLOAT3 col = (_terrainBrush == 1) ? XMFLOAT3{ 1.f, 0.4f, 0.2f } : XMFLOAT3{ 0.3f, 0.9f, 1.f };
		XMFLOAT3 prev{};
		const int seg = 32;
		for (int s = 0; s <= seg; ++s)
		{
			float a = s / (float)seg * 6.2831853f;
			float px = _terrainCursor.x + cosf(a) * _terrainRadius;
			float pz = _terrainCursor.z + sinf(a) * _terrainRadius;
			XMFLOAT3 p{ px, terr->GetHeight(px, pz) + 0.05f, pz };
			if (s > 0) _debugDraw.Line(prev, p, col, /*overlay*/ true);
			prev = p;
		}
		_debugDraw.Cross(XMFLOAT3{ _terrainCursor.x, _terrainCursor.y + 0.05f, _terrainCursor.z }, col, 0.3f);
	}

	// 본 스켈레톤 — 오버레이(깊이 OFF) 배치에 추가해 메시 안에 묻혀도 보이게
	if (_showBones)
		for (size_t b = 0; b < _scene._boneWorld.size(); ++b)
		{ int par = _scene._bonesData[b].parent; if (par >= 0 && par < (int)_scene._boneWorld.size()) _debugDraw.Line(_scene._boneWorld[b], _scene._boneWorld[par], { 0.2f, 1.0f, 1.0f }, /*overlay*/ true); }

	_debugDraw.Flush(_cmdList.Get(), _cb[_frameIndex]->GetGPUVirtualAddress());
}

void D3D12Device::Log(const std::string& m) { _log.push_back(m); if (_log.size() > 200) _log.erase(_log.begin()); }

void D3D12Device::ResetDefaults()
{
	_toonLevels = 0; _rimPower = 0; _normalIntensity = 1; _chroma = _grain = _sharpen = 0;
	_lensDistort = 0; _posterize = 0; _filterMode = 0; _anamorphic = false; _renderScale = 1;
	_contrast = _saturation = 1; _temperature = 0; _vignette = 0.25f; _ev = 0; _fogDensity = 0;
	_aoOn = _dofOn = _volOn = _autoExp = _checker = _scene._terrain = _todOn = false; _wantReload = true;
	_bgMode = 0; _tonemapOp = 0; _exposure = 1;
	_particlesOn = _decalOn = _stars = _flicker = _overlay = false; _letterbox = 0; _cloudAmt = 0;
	_shadowStrength = 1; _hemiAmbient = 0;
	DirectX::XMStoreFloat4x4(&_scene._modelMatrix, DirectX::XMMatrixIdentity());
	Log("Reset scene to defaults");
}

void D3D12Device::PushUndo()
{
	Snapshot s; s.m = _scene._modelMatrix; s.met = _matMetallic; s.rough = _matRoughness; s.emis = _matEmissive; s.tint = _matTint; s.dt = _diffuseTint;
	_undo.push_back(s); if (_undo.size() > 64) _undo.erase(_undo.begin()); _redo.clear();
}
void D3D12Device::DoUndo()
{
	if (_undo.empty()) return;
	Snapshot cur; cur.m = _scene._modelMatrix; cur.met = _matMetallic; cur.rough = _matRoughness; cur.emis = _matEmissive; cur.tint = _matTint; cur.dt = _diffuseTint;
	_redo.push_back(cur);
	Snapshot s = _undo.back(); _undo.pop_back();
	_scene._modelMatrix = s.m; _matMetallic = s.met; _matRoughness = s.rough; _matEmissive = s.emis; _matTint = s.tint; _diffuseTint = s.dt;
	Log("Undo");
}
void D3D12Device::DoRedo()
{
	if (_redo.empty()) return;
	Snapshot cur; cur.m = _scene._modelMatrix; cur.met = _matMetallic; cur.rough = _matRoughness; cur.emis = _matEmissive; cur.tint = _matTint; cur.dt = _diffuseTint;
	_undo.push_back(cur);
	Snapshot s = _redo.back(); _redo.pop_back();
	_scene._modelMatrix = s.m; _matMetallic = s.met; _matRoughness = s.rough; _matEmissive = s.emis; _matTint = s.tint; _diffuseTint = s.dt;
	Log("Redo");
}

// EditorTool 의 ImGuiManager::ApplyEditorStyle 이식 — 차콜 다크 + 블루 액센트 + 라운딩
static void ApplyEditorStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(10, 8); style.FramePadding = ImVec2(8, 4); style.CellPadding = ImVec2(6, 3);
	style.ItemSpacing = ImVec2(8, 5); style.ItemInnerSpacing = ImVec2(6, 4); style.IndentSpacing = 16;
	style.ScrollbarSize = 13; style.GrabMinSize = 10;
	style.WindowBorderSize = 1; style.ChildBorderSize = 1; style.PopupBorderSize = 1; style.FrameBorderSize = 0; style.TabBorderSize = 0;
	style.WindowRounding = 6; style.ChildRounding = 4; style.FrameRounding = 3; style.PopupRounding = 4; style.ScrollbarRounding = 9; style.GrabRounding = 3; style.TabRounding = 4;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f); style.WindowMenuButtonPosition = ImGuiDir_None;
	const ImVec4 accent(0.26f, 0.56f, 0.96f, 1), accentDim(0.22f, 0.42f, 0.69f, 1), accentBright(0.36f, 0.65f, 1, 1);
	ImVec4* c = style.Colors;
	c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.93f, 1); c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.51f, 0.53f, 1);
	c[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1); c[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.13f, 0.14f, 1);
	c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.12f, 0.98f); c[ImGuiCol_Border] = ImVec4(0.25f, 0.26f, 0.29f, 0.60f);
	c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.24f, 1); c[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.27f, 0.30f, 1); c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.34f, 1);
	c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.12f, 1); c[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.15f, 0.18f, 1); c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.11f, 0.12f, 0.75f);
	c[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.13f, 0.14f, 1);
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.11f, 0.12f, 0.13f, 0.60f); c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.35f, 1); c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.38f, 0.42f, 1); c[ImGuiCol_ScrollbarGrabActive] = accentDim;
	c[ImGuiCol_CheckMark] = accentBright; c[ImGuiCol_SliderGrab] = accentDim; c[ImGuiCol_SliderGrabActive] = accentBright;
	c[ImGuiCol_Button] = ImVec4(0.22f, 0.23f, 0.27f, 1); c[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.30f, 0.35f, 1); c[ImGuiCol_ButtonActive] = accentDim;
	c[ImGuiCol_Header] = ImVec4(0.24f, 0.26f, 0.30f, 1); c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.32f, 0.40f, 1); c[ImGuiCol_HeaderActive] = accentDim;
	c[ImGuiCol_Separator] = ImVec4(0.25f, 0.26f, 0.29f, 0.60f); c[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.56f, 0.96f, 0.78f); c[ImGuiCol_SeparatorActive] = accent;
	c[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.56f, 0.96f, 0.20f); c[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.56f, 0.96f, 0.60f); c[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.56f, 0.96f, 0.90f);
	c[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.18f, 1); c[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.42f, 0.66f, 1); c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.30f, 0.45f, 1); c[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.14f, 0.15f, 1); c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.21f, 0.28f, 1);
	c[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.62f, 0.64f, 1); c[ImGuiCol_PlotLinesHovered] = accentBright; c[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.60f, 0.22f, 1); c[ImGuiCol_PlotHistogramHovered] = ImVec4(1, 0.70f, 0.30f, 1);
	c[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.20f, 0.23f, 1); c[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.32f, 0.35f, 1); c[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.24f, 0.26f, 1); c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.56f, 0.96f, 0.35f); c[ImGuiCol_DragDropTarget] = ImVec4(0.36f, 0.65f, 1, 0.90f); c[ImGuiCol_NavHighlight] = accent;
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.50f);
}

void D3D12Device::InitEditor()
{
	RegisterBuiltinScripts(); // 스크립트 팩토리 등록 (Rotator/Bobber)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = nullptr; // imgui.ini 미저장 (런타임 아티팩트 방지)
	ApplyEditorStyle(); // EditorTool 동일 스타일 (차콜 다크 + 블루 액센트)

	ImGui_ImplWin32_Init(_hwnd);
	_imgui.Init(_device.Get(), _queue.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, FRAME_COUNT);
	_thumbnail.Init(_device.Get(), _queue.Get(), &_imgui); // FolderContents 메시/이미지 썸네일

	DirectX::XMStoreFloat4x4(&_viewM, DirectX::XMMatrixIdentity()); // 첫 프레임 ImGuizmo NaN 방지
	DirectX::XMStoreFloat4x4(&_projM, DirectX::XMMatrixIdentity());

	// 에셋 루트 = exe\..\Resources\Assets
	wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
	_assetRoot = fs::weakly_canonical(fs::path(dir) / L".." / L"Resources" / L"Assets").wstring();
	_curDir = _assetRoot;

	CreateSceneRT(_width, _height); // 씬 오프스크린 RT 초기 생성
	_editor.Init(this);             // 에디터 윈도우 등록 (EditorTool 패턴)
	_editorReady = true;
}

// 씬 오프스크린 RT + 깊이 (재)생성 — Scene 창 리사이즈 시 호출 (전체 플러시로 GPU 유휴 보장)
void D3D12Device::CreateSceneRT(UINT w, UINT h)
{
	_sceneW = w; _sceneH = h;

	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 컬러 RT
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = _sceneFmt; rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cvc{}; cvc.Format = rd.Format;
	cvc.Color[0] = 0.06f; cvc.Color[1] = 0.07f; cvc.Color[2] = 0.10f; cvc.Color[3] = 1.0f;
	_sceneRT.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvc, IID_PPV_ARGS(&_sceneRT)), "scene RT");
	_sceneRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	if (!_sceneRtvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_sceneRtvHeap)), "scene RTV heap");
	}
	D3D12_CPU_DESCRIPTOR_HANDLE rtv0 = _sceneRtvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateRenderTargetView(_sceneRT.Get(), nullptr, rtv0); // slot0 = HDR 씬

	// 깊이 (DOF 가 SRV 로 샘플 → PostFX 가 SRV 참조)
	D3D12_RESOURCE_DESC dd{};
	dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1;
	dd.Format = DXGI_FORMAT_R32_TYPELESS; dd.SampleDesc.Count = 1; // SRV(R32_FLOAT)+DSV(D32) 공용
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cvd{}; cvd.Format = DXGI_FORMAT_D32_FLOAT; cvd.DepthStencil.Depth = 1.0f;
	_sceneDepth.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cvd, IID_PPV_ARGS(&_sceneDepth)), "scene depth");
	_sceneDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (!_sceneDsvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_sceneDsvHeap)), "scene DSV heap");
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format = DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_sceneDepth.Get(), &dsv, _sceneDsvHeap->GetCPUDescriptorHandleForHeapStart());

	// 후처리 RT/힙(LDR/LDR2/블룸 + SRV) 재생성 — sceneRT/depth SRV 포함
	_postfx.Resize(w, h, _sceneRT.Get(), _sceneDepth.Get());

	_sceneTexId = _imgui.SetSceneTexture(_postfx.LdrResource()); // ImGui 는 톤맵된 LDR 표시
}

// Game 뷰 오프스크린 RT (게임 카메라 시점) — CreateSceneRT 미러
void D3D12Device::CreateGameRT(UINT w, UINT h)
{
	_gameW = w; _gameH = h;
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = _sceneFmt; rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cvc{}; cvc.Format = rd.Format; cvc.Color[3] = 1.0f;
	_gameRT.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvc, IID_PPV_ARGS(&_gameRT)), "game RT");
	_gameRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (!_gameRtvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_gameRtvHeap)), "game RTV heap"); }
	_device->CreateRenderTargetView(_gameRT.Get(), nullptr, _gameRtvHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RESOURCE_DESC dd{}; dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1; dd.Format = DXGI_FORMAT_R32_TYPELESS; dd.SampleDesc.Count = 1;
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cvd{}; cvd.Format = DXGI_FORMAT_D32_FLOAT; cvd.DepthStencil.Depth = 1.0f;
	_gameDepth.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cvd, IID_PPV_ARGS(&_gameDepth)), "game depth");
	_gameDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (!_gameDsvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_gameDsvHeap)), "game DSV heap"); }
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format = DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_gameDepth.Get(), &dsv, _gameDsvHeap->GetCPUDescriptorHandleForHeapStart());

	_gamePostfx.Resize(w, h, _gameRT.Get(), _gameDepth.Get());
	_gameTexId = _imgui.SetGameTexture(_gamePostfx.LdrResource());
}

// "Game" 도킹 창 — 게임 카메라 렌더 결과 표시 (DrawSceneView 끝에서 호출)
void D3D12Device::DrawGameView()
{
	ImGui::Begin("Game");
	_gameWindowOpen = !ImGui::IsWindowCollapsed();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	_pendingGameW = (UINT)max(8.0f, avail.x);
	_pendingGameH = (UINT)max(8.0f, avail.y);
	if (_gameTexId) ImGui::Image((ImTextureID)_gameTexId, avail);
	ImGui::End();
}

// ImGui NewFrame ~ Render 는 EditorManager 가 담당 (메뉴바/도킹 호스트/패널 순회/Render)
void D3D12Device::BuildUI()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
	_thumbnail.NewFrame(4); // 프레임당 신규 썸네일 생성 제한 (전체플러시 스터터 방지)
	_editor.Update();
}

// 메인 메뉴바 (EditorTool 구조: File / Edit / GameObject / Component / View) — MainMenuBarWindow 가 호출
void D3D12Device::DrawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N")) NewScene();
			if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) LoadScene();
			if (ImGui::MenuItem("Save Scene...", "Ctrl+S")) SaveScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Convert FBX...")) ConvertFbxDialog(); // ufbx → .mesh/.clip/.mat 변환 후 스폰
			ImGui::Separator();
			if (ImGui::MenuItem("Screenshot (PNG)")) _wantShot = true;
			if (ImGui::MenuItem("Screenshot Hi-Res 2x")) { _renderScale = 2.0f; _wantShot = true; _hiresShot = true; }
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Alt+F4")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !_undo.empty())) DoUndo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !_redo.empty())) DoRedo();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("GameObject"))
		{
			Vec3 sp = SpawnPoint();
			if (ImGui::MenuItem("Create Empty", "Ctrl+B")) SpawnEmpty(L"GameObject", sp);
			if (ImGui::BeginMenu("3D Object"))
			{
				vector<Vtx> v; vector<uint32> idx;
				if (ImGui::MenuItem("Cube"))   { BuildPrim(MeshPrim::Cube, v, idx);   SpawnMeshObject(L"Cube", v, idx, sp, MeshPrim::Cube); }
				if (ImGui::MenuItem("Sphere")) { BuildPrim(MeshPrim::Sphere, v, idx); SpawnMeshObject(L"Sphere", v, idx, sp, MeshPrim::Sphere); }
				if (ImGui::MenuItem("Plane"))  { BuildPrim(MeshPrim::Plane, v, idx);  SpawnMeshObject(L"Plane", v, idx, Vec3{ sp.x,0,sp.z }, MeshPrim::Plane); }
				ImGui::Separator();
				if (ImGui::MenuItem("Animated Model (Archer)"))
					SpawnAnimatedModel(_assetRoot + L"\\Models\\Archer\\Archer.mesh", Vec3{ sp.x, 0, sp.z });
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Directional Light")) { _sel = SelEntity::Sun; _selectedGO = nullptr; } // 디렉셔널=단일 sun(선택)
			if (ImGui::MenuItem("Point Light"))       SpawnLight(1, L"Point Light", sp);
			if (ImGui::MenuItem("Spot Light"))        SpawnLight(2, L"Spot Light", sp);
			if (ImGui::MenuItem("Particle System"))   { auto o = SpawnEmpty(L"Particles", sp); if (o) o->AddComponent(make_shared<ParticleSystem>()); }
			if (ImGui::MenuItem("Camera")) { auto o = SpawnEmpty(L"Camera", _camera.pos); if (o) { o->AddComponent(make_shared<Camera>()); if (auto t = o->GetTransform()) t->SetLocalRotation(Vec3{ _camera.pitch, _camera.yaw, 0.f }); } }
			ImGui::Separator();
			if (ImGui::MenuItem("Terrain")) SpawnTerrain(128, 1.0f);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Component"))
		{
			if (ImGui::MenuItem("Mesh")) _sel = SelEntity::Model;
			ImGui::MenuItem("Sound", nullptr, false, false);
			if (ImGui::MenuItem("Light")) _sel = SelEntity::Sun;
			if (ImGui::MenuItem("Camera")) _sel = SelEntity::Camera;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", nullptr, &_showGrid);
			ImGui::MenuItem("Sky", nullptr, &_showSky);
			ImGui::MenuItem("Bloom", nullptr, &_bloomOn);
			ImGui::MenuItem("Wireframe", nullptr, &_wireframe);
			ImGui::EndMenu();
		}
		float fps = ImGui::GetIO().Framerate;
		char stat[128];
		snprintf(stat, sizeof(stat), "%.1f FPS  |  %u tris  |  %u probes  |  DXR RT", fps, _scene._indexCount / 3, Ddgi::PROBE_COUNT);
		float tw = ImGui::CalcTextSize(stat).x;
		ImGui::SameLine(ImGui::GetWindowWidth() - tw - 16.0f);
		ImGui::TextDisabled("%s", stat);
		ImGui::EndMainMenuBar();
	}

	// 전역 단축키 (메뉴 라벨 Ctrl+S/Ctrl+O 실제 동작) — 텍스트 입력 중 제외
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantTextInput && io.KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_S, false)) SaveScene();
		else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) LoadScene();
		else if (ImGui::IsKeyPressed(ImGuiKey_N, false)) NewScene();
	}
}

void D3D12Device::DrawHierarchy()
{
	ImGui::Begin("Hierarchy");
	ImGui::TextDisabled("Scene");
	ImGui::Separator();

	std::string modelItem = "[Mdl] " + WToUtf8(_scene._modelLabel);
	struct Entry { std::string label; SelEntity e; };
	const Entry items[] = {
		{ "[Cam] Editor Camera", SelEntity::Camera },
		{ "[Sun] Directional Light", SelEntity::Sun },
		{ "[GI]  DDGI Volume", SelEntity::DDGI },
		{ "[Pt]  Point Light", SelEntity::Point },
		{ "[Spt] Spot Light", SelEntity::Spot },
		{ "[FX]  Post / Render", SelEntity::Post },
		{ modelItem, SelEntity::Model },
		{ "[Geo] Floor", SelEntity::Floor },
	};
	for (const Entry& it : items)
	{
		if (ImGui::Selectable(it.label.c_str(), _sel == it.e && !_selectedGO))
		{ _sel = it.e; _selectedGO = nullptr; } // 고정 엔티티 선택 시 GameObject 선택 해제
	}

	// ── 씬 그래프 GameObject 목록 (EditorTool Hierarchy 대응) ──
	ImGui::Separator();
	ImGui::TextDisabled("GameObjects  (drag = reparent)");
	if (_gameScene)
	{
		// 재귀 트리 (루트 = 부모 없음) + 드래그드롭 부모지정
		std::function<void(const shared_ptr<GameObject>&)> drawNode = [&](const shared_ptr<GameObject>& obj)
		{
			if (!obj) return;
			auto t = obj->GetTransform();
			bool hasChildren = t && !t->GetChildren().empty();
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
			if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
			if (_selectedGO == obj) flags |= ImGuiTreeNodeFlags_Selected;
			std::string label = WToUtf8(obj->GetObjectName());
			bool open = ImGui::TreeNodeEx((void*)(intptr_t)obj->GetId(), flags, "%s", label.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) _selectedGO = obj;
			if (ImGui::BeginDragDropSource())
			{
				int64 id = obj->GetId();
				ImGui::SetDragDropPayload("GO_ID", &id, sizeof(id));
				ImGui::TextUnformatted(label.c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("GO_ID"))
				{
					int64 id = *(const int64*)pl->Data;
					if (auto dragged = _gameScene->GetCreatedObject(id); dragged && dragged != obj)
						if (auto dt = dragged->GetTransform(), pt = obj->GetTransform(); dt && pt)
							dt->SetParentKeepWorld(pt); // 월드 유지 재부모
				}
				ImGui::EndDragDropTarget();
			}
			if (open)
			{
				if (hasChildren)
				{
					std::vector<shared_ptr<GameObject>> kids; // 재부모가 자식목록을 변경하므로 스냅샷
					for (auto& ct : t->GetChildren())
						if (ct) if (auto cgo = ct->GetGameObject()) kids.push_back(cgo);
					for (auto& k : kids) drawNode(k);
				}
				ImGui::TreePop();
			}
		};
		std::vector<shared_ptr<GameObject>> roots;
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second; if (!obj) continue;
			auto t = obj->GetTransform();
			if (!t || !t->GetParent()) roots.push_back(obj);
		}
		for (auto& r : roots) drawNode(r);

		// 빈 공간 = 부모 해제 (루트로)
		ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 24));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("GO_ID"))
			{
				int64 id = *(const int64*)pl->Data;
				if (auto dragged = _gameScene->GetCreatedObject(id))
					if (auto dt = dragged->GetTransform()) dt->SetParentKeepWorld(nullptr);
			}
			ImGui::EndDragDropTarget();
		}
	}

	// 우클릭 컨텍스트 메뉴 (EditorTool GameObject 생성 흐름)
	if (ImGui::BeginPopupContextWindow("hctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		Vec3 spawnAt = SpawnPoint(); // 카메라 앞
		if (ImGui::MenuItem("Create Empty")) SpawnEmpty(L"GameObject", spawnAt);
		if (ImGui::BeginMenu("3D Object"))
		{
			vector<Vtx> v; vector<uint32> idx;
			if (ImGui::MenuItem("Cube"))   { BuildPrim(MeshPrim::Cube, v, idx);   SpawnMeshObject(L"Cube", v, idx, spawnAt, MeshPrim::Cube); }
			if (ImGui::MenuItem("Sphere")) { BuildPrim(MeshPrim::Sphere, v, idx); SpawnMeshObject(L"Sphere", v, idx, spawnAt, MeshPrim::Sphere); }
			if (ImGui::MenuItem("Plane"))  { BuildPrim(MeshPrim::Plane, v, idx);  SpawnMeshObject(L"Plane", v, idx, Vec3{ spawnAt.x, 0, spawnAt.z }, MeshPrim::Plane); }
			ImGui::Separator();
			if (ImGui::MenuItem("Animated Model (Archer)"))
				SpawnAnimatedModel(_assetRoot + L"\\Models\\Archer\\Archer.mesh", Vec3{ spawnAt.x, 0, spawnAt.z });
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Directional Light")) { _sel = SelEntity::Sun; _selectedGO = nullptr; } // 디렉셔널=단일 sun(선택)
		if (ImGui::MenuItem("Point Light"))       SpawnLight(1, L"Point Light", spawnAt);
		if (ImGui::MenuItem("Spot Light"))        SpawnLight(2, L"Spot Light", spawnAt);
		if (ImGui::MenuItem("Particle System"))   { auto o = SpawnEmpty(L"Particles", spawnAt); if (o) o->AddComponent(make_shared<ParticleSystem>()); }
		if (ImGui::MenuItem("Camera")) { auto o = SpawnEmpty(L"Camera", _camera.pos); if (o) { o->AddComponent(make_shared<Camera>()); if (auto t = o->GetTransform()) t->SetLocalRotation(Vec3{ _camera.pitch, _camera.yaw, 0.f }); } }
		ImGui::Separator();
		bool hasSel = _selectedGO != nullptr;
		if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSel)) DuplicateSelectedObject();
		if (ImGui::MenuItem("Delete", "Del", false, hasSel)) DeleteSelectedObject();
		ImGui::EndPopup();
	}

	// 단축키: Delete = 삭제, Ctrl+D = 복제 (씬뷰/하이어라키 포커스 시)
	if (_selectedGO && !ImGui::GetIO().WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelectedObject();
		else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) DuplicateSelectedObject();
	}
	ImGui::End();
}

void D3D12Device::DrawSceneView()
{
	ImGui::Begin("Scene");
	// ── Play/Stop ──
	{
		ImVec4 pc = _playing ? ImVec4(0.8f, 0.3f, 0.2f, 1.f) : ImVec4(0.2f, 0.6f, 0.3f, 1.f);
		ImGui::PushStyleColor(ImGuiCol_Button, pc);
		if (ImGui::Button(_playing ? "■ Stop" : "▶ Play")) TogglePlay();
		ImGui::PopStyleColor();
		ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
	}
	// ── 기즈모 툴바 (EditorTool 씬뷰 스타일) ──
	if (ImGui::RadioButton("Move(W)", _gizmoOp == 7)) _gizmoOp = 7; ImGui::SameLine();
	if (ImGui::RadioButton("Rotate(E)", _gizmoOp == 120)) _gizmoOp = 120; ImGui::SameLine();
	if (ImGui::RadioButton("Scale(R)", _gizmoOp == 896)) _gizmoOp = 896; ImGui::SameLine();
	ImGui::TextDisabled("|"); ImGui::SameLine();
	if (ImGui::RadioButton("Local", _gizmoLocal)) _gizmoLocal = true; ImGui::SameLine();
	if (ImGui::RadioButton("World", !_gizmoLocal)) _gizmoLocal = false; ImGui::SameLine();
	ImGui::Checkbox("Snap", &_snapOn);
	ImVec2 avail = ImGui::GetContentRegionAvail();
	_pendingSceneW = (UINT)max(8.0f, avail.x);
	_pendingSceneH = (UINT)max(8.0f, avail.y);
	ImVec2 imgPos = ImGui::GetCursorScreenPos();
	if (_sceneTexId)
		ImGui::Image((ImTextureID)_sceneTexId, avail);

	// ── .mesh 드래그드롭 → 바닥 히트 지점에 배치 ──
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MESH_PATH"))
		{
			using namespace DirectX;
			std::wstring meshPath = Utf8ToW(std::string((const char*)pl->Data));
			ImVec2 mp = ImGui::GetMousePos();
			float u = (mp.x - imgPos.x) / avail.x, v = (mp.y - imgPos.y) / avail.y;
			Vec3 pos = SpawnPoint(); pos.y = 0.f; // 폴백
			float nx = u * 2.f - 1.f, ny = (1.f - v) * 2.f - 1.f;
			XMMATRIX invVP = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_viewM) * XMLoadFloat4x4(&_projM));
			XMVECTOR n = XMVector4Transform(XMVectorSet(nx, ny, 0, 1), invVP); n = XMVectorScale(n, 1.f / XMVectorGetW(n));
			XMVECTOR f = XMVector4Transform(XMVectorSet(nx, ny, 1, 1), invVP); f = XMVectorScale(f, 1.f / XMVectorGetW(f));
			XMFLOAT3 ro; XMStoreFloat3(&ro, n);
			XMFLOAT3 rd; XMStoreFloat3(&rd, XMVector3Normalize(XMVectorSubtract(f, n)));
			if (fabsf(rd.y) > 1e-6f) { float t = -ro.y / rd.y; if (t > 0) pos = Vec3{ ro.x + rd.x * t, 0.f, ro.z + rd.z * t }; }
			SpawnAnimatedModel(meshPath, pos);
		}
		ImGui::EndDragDropTarget();
	}

	_sceneHovered = ImGui::IsWindowHovered();
	_sceneFocused = ImGui::IsWindowFocused();

	// ── 터레인 편집 모드: 좌드래그 = 스컬프트 (선택 GameObject 에 Terrain 컴포넌트 있을 때) ──
	bool terrainEditing = _terrainEdit && _selectedGO && _selectedGO->GetComponent<Terrain>();
	if (terrainEditing && _sceneHovered)
	{
		ImVec2 mp = ImGui::GetMousePos();
		float u = (mp.x - imgPos.x) / avail.x, v = (mp.y - imgPos.y) / avail.y;
		if (u >= 0 && u <= 1 && v >= 0 && v <= 1)
		{
			bool down = ImGui::IsMouseDown(0);
			TerrainBrushAt(u, v, down); // 다운 시 스컬프트, 아니면 커서만
		}
	}
	// ── 클릭 픽킹 (좌클릭, 기즈모 위 아닐 때 — 터레인 편집 중엔 비활성) ──
	else if (_sceneHovered && ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
	{
		ImVec2 mp = ImGui::GetMousePos();
		float u = (mp.x - imgPos.x) / avail.x, v = (mp.y - imgPos.y) / avail.y;
		if (u >= 0 && u <= 1 && v >= 0 && v <= 1) PickAt(u, v);
	}

	// ── ImGuizmo: 선택 GameObject / 모델 / 점광원 트랜스폼 조작 (이미지 영역 오버레이) ──
	bool goGizmo = _selectedGO && _selectedGO != _modelObj && _selectedGO->GetTransform() && !_selectedGO->IsEditorInternal() && !terrainEditing;
	if (goGizmo || (_sel == SelEntity::Model && _scene._modelMatrixInit) || _sel == SelEntity::Point)
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetGizmoSizeClipSpace(_gizmoSize); // V6
		ImGuizmo::SetRect(imgPos.x, imgPos.y, avail.x, avail.y);
		if (goGizmo)
		{
			auto t = _selectedGO->GetTransform();
			DirectX::XMFLOAT4X4 wm = t->GetWorldMatrix();
			float snapVal = (_gizmoOp == 7) ? _snapT : (_gizmoOp == 120) ? _snapR : _snapS;
			float snap3[3] = { snapVal, snapVal, snapVal };
			if (ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				(ImGuizmo::OPERATION)_gizmoOp, _gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
				(float*)&wm, nullptr, _snapOn ? snap3 : nullptr))
				t->SetWorldMatrix(wm); // 월드→로컬 역산 + 분해 (부모 회전/스케일 안전)
		}
		else if (_sel == SelEntity::Model)
		{
			float snapVal = (_gizmoOp == 7) ? _snapT : (_gizmoOp == 120) ? _snapR : _snapS;
			float snap3[3] = { snapVal, snapVal, snapVal };
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				(ImGuizmo::OPERATION)_gizmoOp, _gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
				(float*)&_scene._modelMatrix, nullptr, _snapOn ? snap3 : nullptr);
		}
		else // Point — 이동만
		{
			DirectX::XMFLOAT4X4 m; DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixTranslation(_pointPos.x, _pointPos.y, _pointPos.z));
			ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
				ImGuizmo::TRANSLATE, ImGuizmo::WORLD, (float*)&m);
			_pointPos = { m._41, m._42, m._43 };
		}
		static bool wasUsing = false;
		bool using_ = ImGuizmo::IsUsing();
		if (using_ && !wasUsing && _sel == SelEntity::Model) PushUndo(); // 조작 시작 시 되돌리기 지점
		wasUsing = using_;
	}

	// W4 레터박스 + W5 오버레이 (씬 이미지 위 드로우리스트)
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 a = imgPos, b = ImVec2(imgPos.x + avail.x, imgPos.y + avail.y);
	if (_letterbox > 0.001f)
	{
		float barH = avail.y * _letterbox * 0.5f;
		dl->AddRectFilled(a, ImVec2(b.x, a.y + barH), IM_COL32(0, 0, 0, 255));
		dl->AddRectFilled(ImVec2(a.x, b.y - barH), b, IM_COL32(0, 0, 0, 255));
	}
	if (_overlay)
	{
		ImU32 c = IM_COL32(255, 255, 255, 70);
		for (int k = 1; k < 3; ++k)
		{
			float x = a.x + avail.x * k / 3.0f, y = a.y + avail.y * k / 3.0f;
			dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), c); dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), c);
		}
		ImVec2 ct((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
		dl->AddLine(ImVec2(ct.x - 11, ct.y), ImVec2(ct.x + 11, ct.y), IM_COL32(255, 255, 255, 170));
		dl->AddLine(ImVec2(ct.x, ct.y - 11), ImVec2(ct.x, ct.y + 11), IM_COL32(255, 255, 255, 170));
	}
	ImGui::End();

	DrawGameView(); // "Game" 도킹 창 (게임 카메라 시점)
}

void D3D12Device::DrawLog()
{
	ImGui::Begin("Log");
	if (ImGui::Button("Clear")) _log.clear();
	ImGui::Separator();
	ImGui::BeginChild("logscroll");
	for (auto& m : _log) ImGui::TextUnformatted(m.c_str());
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::End();
}

// ── 스크린샷 (T19) — PostFX LDR 리드백 → PNG (exe 폴더) ──
void D3D12Device::SaveScreenshot()
{
	ID3D12Resource* ldr = _postfx.LdrResource();
	if (!ldr) return;
	D3D12_RESOURCE_DESC td = ldr->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
	_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);

	D3D12_HEAP_PROPERTIES rb{}; rb.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
	bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> readback;
	if (FAILED(_device->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return;

	ComPtr<ID3D12CommandAllocator> al; ComPtr<ID3D12GraphicsCommandList> cl;
	_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al.Get(), nullptr, IID_PPV_ARGS(&cl));
	D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = ldr;
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; cl->ResourceBarrier(1, &b);
	D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
	D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = ldr; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
	cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	std::swap(b.Transition.StateBefore, b.Transition.StateAfter); cl->ResourceBarrier(1, &b);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() }; _queue->ExecuteCommandLists(1, lists);
	WaitForGpu();

	uint8_t* data = nullptr; D3D12_RANGE rr{ 0, (SIZE_T)total };
	if (FAILED(readback->Map(0, &rr, (void**)&data))) return;
	UINT W = (UINT)td.Width, H = td.Height;
	std::vector<uint8_t> rgba((size_t)W * H * 4);
	for (UINT y = 0; y < H; ++y)
		memcpy(rgba.data() + (size_t)y * W * 4, data + fp.Offset + (size_t)y * fp.Footprint.RowPitch, (size_t)W * 4);
	readback->Unmap(0, nullptr);

	// PNG 작성 (WIC)
	static int idx = 0;
	wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
	std::wstring path = dir + L"\\screenshot_" + std::to_wstring(idx++) + L".png";

	ComPtr<IWICImagingFactory> wic;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return;
	ComPtr<IWICStream> stream; wic->CreateStream(&stream);
	if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) return;
	ComPtr<IWICBitmapEncoder> enc; wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
	enc->Initialize(stream.Get(), WICBitmapEncoderNoCache);
	ComPtr<IWICBitmapFrameEncode> frame; ComPtr<IPropertyBag2> props;
	enc->CreateNewFrame(&frame, &props); frame->Initialize(props.Get());
	frame->SetSize(W, H);
	WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA; frame->SetPixelFormat(&fmt);
	frame->WritePixels(H, W * 4, (UINT)rgba.size(), rgba.data());
	frame->Commit(); enc->Commit();
	Log("Screenshot saved: " + WToUtf8(fs::path(path).filename().wstring()));
}

// ── 씬그래프 편집: GameObject 생성/삭제/복제 (하이어라키) ──
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
static void BuildPrim(MeshPrim prim, vector<Vtx>& v, vector<uint32>& idx)
{
	switch (prim) {
	case MeshPrim::Sphere: GeometryHelper::CreateSphere(v, idx, 0.5f); break;
	case MeshPrim::Plane:  GeometryHelper::CreatePlane(v, idx, 2.0f);  break;
	case MeshPrim::Cube:
	default:               GeometryHelper::CreateCube(v, idx, 1.0f);   break;
	}
}

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
	if (!_selectedGO || !_gameScene) return;
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

void D3D12Device::DuplicateSelectedObject()
{
	auto source = _selectedGO; // SpawnMeshObject 가 _selectedGO 를 재할당하므로 먼저 캡처
	if (!source || !_gameScene) return;

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
	f << "mat " << _matMetallic << ' ' << _matRoughness << ' ' << _matEmissive << ' ' << _matTint << '\n';
	f << "model " << WToUtf8((_scene._modelDir + _scene._modelStem + L".mesh")) << '\n';
	f << "xform";
	const float* m = &_scene._modelMatrix._11;
	for (int i = 0; i < 16; ++i) f << ' ' << m[i];
	f << '\n';

	// ── 멀티 오브젝트: 씬그래프의 MeshRenderer GameObject 들 (트랜스폼 + 머티리얼 + 텍스처) ──
	int meshCount = 0, lightCount = 0, animCount = 0;
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
	}
	f.flush();
	Log("Scene saved: " + WToUtf8(path) + "  (" + std::to_string(meshCount) + " mesh, "
		+ std::to_string(lightCount) + " light, " + std::to_string(animCount) + " anim)");
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
		else if (tag == "mat") s >> _matMetallic >> _matRoughness >> _matEmissive >> _matTint;
		else if (tag == "model") { std::getline(s >> std::ws, modelUtf8); }
		else if (tag == "xform") { float* m = &_pendingMatrix._11; for (int i = 0; i < 16; ++i) s >> m[i]; _hasPendingMatrix = true; }
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
		_pendingModel = wp; // 다음 프레임 GPU 유휴 시 로드 + _pendingMatrix 적용
	}
	Log("Scene loaded: " + WToUtf8(path));
}

// 씬뷰 클릭 레이 → 모델 AABB / 바닥 평면 픽킹 → 하이어라키 선택
void D3D12Device::PickAt(float u, float v)
{
	using namespace DirectX;
	float nx = u * 2.0f - 1.0f, ny = (1.0f - v) * 2.0f - 1.0f;
	XMMATRIX invVP = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_viewM) * XMLoadFloat4x4(&_projM));
	XMVECTOR n = XMVector4Transform(XMVectorSet(nx, ny, 0, 1), invVP);
	XMVECTOR f = XMVector4Transform(XMVectorSet(nx, ny, 1, 1), invVP);
	n = XMVectorScale(n, 1.0f / XMVectorGetW(n));
	f = XMVectorScale(f, 1.0f / XMVectorGetW(f));
	XMFLOAT3 ro; XMStoreFloat3(&ro, n);
	XMFLOAT3 rdv; XMStoreFloat3(&rdv, XMVector3Normalize(XMVectorSubtract(f, n)));

	// 모델 AABB 슬랩 테스트
	float tModel = 1e9f; bool hitM = false;
	{
		float tmin = 0.0f, tmax = 1e9f; bool ok = true;
		const float* ros = &ro.x; const float* rds = &rdv.x;
		const float* bmn = &_scene._modelMin.x; const float* bmx = &_scene._modelMax.x;
		for (int a = 0; a < 3; ++a)
		{
			if (fabsf(rds[a]) < 1e-7f) { if (ros[a] < bmn[a] || ros[a] > bmx[a]) { ok = false; break; } }
			else { float inv = 1.0f / rds[a]; float t1 = (bmn[a] - ros[a]) * inv, t2 = (bmx[a] - ros[a]) * inv;
				if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
				tmin = max(tmin, t1); tmax = min(tmax, t2); if (tmin > tmax) { ok = false; break; } }
		}
		if (ok && tmax > 0) { hitM = true; tModel = max(tmin, 0.0f); }
	}
	// 바닥 평면 y=0 (±6)
	float tFloor = 1e9f; bool hitF = false;
	if (fabsf(rdv.y) > 1e-6f)
	{
		float t = -ro.y / rdv.y;
		if (t > 0) { float hx = ro.x + rdv.x * t, hz = ro.z + rdv.z * t;
			if (fabsf(hx) <= 6.0f && fabsf(hz) <= 6.0f) { hitF = true; tFloor = t; } }
	}

	// 씬그래프 MeshRenderer GameObject AABB (월드 — SortGameObject 가 매 프레임 갱신)
	shared_ptr<GameObject> hitGO; float tGO = 1e9f;
	if (_gameScene)
	{
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& obj = kv.second;
			if (!obj || obj == _modelObj || obj->IsEditorInternal() || !obj->IsActive() || !obj->GetUIPickable()) continue;
			// 콜라이더 있으면 정밀 교차 우선
			if (auto col = obj->GetComponent<BaseCollider>())
			{
				float cd = 1e9f;
				if (col->Intersects(ro, rdv, cd) && cd < tGO) { tGO = cd; hitGO = obj; }
				continue;
			}
			auto r = obj->GetRenderer(); if (!r) continue; // 메시/애니메이터 공통(콜라이더 없을 때)
			const DirectX::BoundingBox& bb = r->GetBoundingBox();
			float bmn[3] = { bb.Center.x - bb.Extents.x, bb.Center.y - bb.Extents.y, bb.Center.z - bb.Extents.z };
			float bmx[3] = { bb.Center.x + bb.Extents.x, bb.Center.y + bb.Extents.y, bb.Center.z + bb.Extents.z };
			float tmin = 0.0f, tmax = 1e9f; bool ok = true;
			const float* ros = &ro.x; const float* rds = &rdv.x;
			for (int a = 0; a < 3; ++a)
			{
				if (fabsf(rds[a]) < 1e-7f) { if (ros[a] < bmn[a] || ros[a] > bmx[a]) { ok = false; break; } }
				else { float inv = 1.0f / rds[a]; float t1 = (bmn[a] - ros[a]) * inv, t2 = (bmx[a] - ros[a]) * inv;
					if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
					tmin = max(tmin, t1); tmax = min(tmax, t2); if (tmin > tmax) { ok = false; break; } }
			}
			if (ok && tmax > 0) { float td = max(tmin, 0.0f); if (td < tGO) { tGO = td; hitGO = obj; } }
		}
	}

	// 가장 가까운 히트 선택 (씬그래프 오브젝트 > 모델 > 바닥)
	if (hitGO && tGO <= tModel && (!hitF || tGO <= tFloor)) { _selectedGO = hitGO; _sel = SelEntity::Model; }
	else if (hitM && (!hitF || tModel <= tFloor)) { _selectedGO = nullptr; _sel = SelEntity::Model; }
	else if (hitF) { _selectedGO = nullptr; _sel = SelEntity::Floor; }
}

void D3D12Device::DrawInspector()
{
	ImGui::Begin("Inspector");

	// ── GameObject 선택 시: 컴포넌트 기반 인스펙터 (EditorTool 대응) ──
	if (_selectedGO)
	{
		DrawGameObjectInspector(_selectedGO);
		ImGui::End();
		return;
	}

	// 하이어라키 선택 대상별 프로퍼티 (고정 엔티티)
	switch (_sel)
	{
	case SelEntity::Camera:
		ImGui::SeparatorText("Editor Camera");
		ImGui::Text("Pos   %.2f  %.2f  %.2f", _camera.pos.x, _camera.pos.y, _camera.pos.z);
		ImGui::Text("Yaw %.2f   Pitch %.2f", _camera.yaw, _camera.pitch);
		ImGui::SliderFloat("FOV", &_camera.fov, 25.0f, 100.0f);              // T1
		ImGui::SliderFloat("Move Speed", &_camera.moveSpeed, 0.5f, 20.0f);   // T1
		if (ImGui::Button("Reset Camera"))                              // T1
		{ _camera.pos = { 3.4f, 2.4f, -4.6f }; _camera.yaw = -0.637f; _camera.pitch = -0.232f; _camera.fov = 55.0f; }
		ImGui::SliderFloat("Near", &_camera.nearZ, 0.01f, 2.0f);            // V7
		ImGui::SliderFloat("Far", &_camera.farZ, 20.0f, 500.0f);
		ImGui::Checkbox("Auto Orbit", &_camera.orbit);
		ImGui::SliderFloat("EV", &_ev, -4.0f, 4.0f);                    // V13
		ImGui::SliderFloat("Gizmo Size", &_gizmoSize, 0.05f, 0.25f);    // V6
		ImGui::SeparatorText("Bookmarks");                              // V8
		for (int k = 0; k < 4; ++k)
		{
			ImGui::PushID(k);
			if (ImGui::Button("Set")) { _camera.bm[k] = { _camera.pos, _camera.yaw, _camera.pitch, true }; }
			ImGui::SameLine(); if (ImGui::Button("Go") && _camera.bm[k].set) { _camera.pos = _camera.bm[k].pos; _camera.yaw = _camera.bm[k].yaw; _camera.pitch = _camera.bm[k].pitch; }
			ImGui::SameLine(); ImGui::Text("Cam %d %s", k + 1, _camera.bm[k].set ? "*" : "");
			ImGui::PopID();
		}
		ImGui::TextDisabled("RMB fly: WASD/QE/Shift, W/E/R gizmo, F focus");
		break;

	case SelEntity::Sun:
		ImGui::SeparatorText("Directional Light");                      // T5
		ImGui::SliderFloat("Intensity", &_lightIntensity, 0.0f, 4.0f);
		ImGui::ColorEdit3("Sun Color", &_sunColor.x);
		ImGui::SliderFloat("Env Intensity", &_envIntensity, 0.0f, 3.0f);
		ImGui::Checkbox("Animate Sun", &_lightAnimate);
		if (!_lightAnimate) ImGui::SliderFloat("Sun Angle", &_lightAngle, -3.14159f, 3.14159f);
		ImGui::SliderFloat("Shadow Softness", &_shadowSoft, 0.0f, 0.12f); // T11
		ImGui::Checkbox("Time of Day", &_todOn);                          // V4
		if (_todOn) ImGui::SliderFloat("Hour", &_timeOfDay, 0.0f, 1.0f);
		ImGui::SeparatorText("Sky");                                    // T20
		ImGui::ColorEdit3("Zenith", &_skyZenith.x);
		ImGui::ColorEdit3("Horizon", &_skyHorizon.x);
		ImGui::SliderFloat("Sun Size", &_sunSize, 50.0f, 4000.0f);
		ImGui::SliderFloat("Shadow Strength", &_shadowStrength, 0.0f, 1.0f); // W6
		ImGui::SliderFloat("Hemi Ambient", &_hemiAmbient, 0.0f, 1.0f);       // W7
		ImGui::Checkbox("Night Stars", &_stars);                            // W8
		ImGui::SliderFloat("Clouds", &_cloudAmt, 0.0f, 1.0f);               // W3
		ImGui::TextDisabled("Presets:");                               // U8
		ImGui::SameLine(); if (ImGui::Button("Day"))    { _skyZenith = {0.13f,0.22f,0.44f}; _skyHorizon = {0.52f,0.60f,0.72f}; _sunColor = {1,0.96f,0.88f}; _lightIntensity = 1.6f; _lightAngle = 0.6f; }
		ImGui::SameLine(); if (ImGui::Button("Sunset")) { _skyZenith = {0.22f,0.15f,0.32f}; _skyHorizon = {0.95f,0.5f,0.22f}; _sunColor = {1.0f,0.55f,0.28f}; _lightIntensity = 1.3f; _lightAngle = 1.45f; _lightAnimate = false; }
		ImGui::SameLine(); if (ImGui::Button("Night"))  { _skyZenith = {0.02f,0.03f,0.09f}; _skyHorizon = {0.06f,0.08f,0.14f}; _sunColor = {0.4f,0.5f,0.7f}; _lightIntensity = 0.25f; }
		break;

	case SelEntity::DDGI:
		ImGui::SeparatorText("DDGI Volume");
		ImGui::SliderFloat("GI Strength", &_giStrength, 0.0f, 1.5f);
		ImGui::SliderFloat("Ambient", &_ambient, 0.0f, 0.2f);
		ImGui::Checkbox("Show Probes", &_probeViz);                      // T15
		ImGui::Spacing();
		ImGui::Text("Probes : %u  (%dx%dx%d)", Ddgi::PROBE_COUNT, Ddgi::PROBE_X, Ddgi::PROBE_Y, Ddgi::PROBE_Z);
		ImGui::TextDisabled("SH-L1 + Chebyshev oct-depth visibility");
		break;

	case SelEntity::Point:
	{
		ImGui::SeparatorText("Point Lights");                           // T14
		ImGui::Checkbox("Enabled (Light 0)", &_pointOn);
		ImGui::SliderInt("Light Count", &_ptCount, 1, MAX_PT);
		ImGui::Separator();
		ImGui::TextDisabled("Light 0 (gizmo-movable)");
		ImGui::DragFloat3("Position##0", &_pointPos.x, 0.05f);
		ImGui::ColorEdit3("Color##0", &_pointColor.x);
		ImGui::SliderFloat("Intensity##0", &_pointIntensity, 0.0f, 12.0f);
		ImGui::SliderFloat("Radius##0", &_pointRadius, 0.5f, 20.0f);
		ImGui::Checkbox("Orbit##0", &_ptOrbit); ImGui::SameLine(); ImGui::SliderFloat("Orbit Spd", &_ptOrbitSpeed, 0.1f, 3.0f); // V14
		ImGui::Checkbox("Flicker", &_flicker); // W9
		for (int li = 1; li < _ptCount; ++li)
		{
			ImGui::PushID(li);
			ImGui::Separator(); ImGui::TextDisabled("Light %d", li);
			if (_ptPosArr[li].w < 0.01f) { _ptPosArr[li] = { (float)li * 1.5f, 2.0f, -1.0f, 7.0f }; _ptColArr[li] = { 2.0f, 1.0f, 0.6f, 1.0f }; }
			ImGui::DragFloat3("Position", &_ptPosArr[li].x, 0.05f);
			ImGui::SliderFloat("Radius", &_ptPosArr[li].w, 0.5f, 20.0f);
			ImGui::ColorEdit3("Color x Int", &_ptColArr[li].x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			_ptColArr[li].w = 1.0f;
			ImGui::PopID();
		}
		break;
	}

	case SelEntity::Spot:                                               // T13
		ImGui::SeparatorText("Spot Light");
		ImGui::Checkbox("Enabled", &_spotOn);
		ImGui::DragFloat3("Position", &_spotPos.x, 0.05f);
		ImGui::DragFloat3("Direction", &_spotDir.x, 0.02f);
		ImGui::ColorEdit3("Color", &_spotColor.x);
		ImGui::SliderFloat("Intensity", &_spotIntensity, 0.0f, 16.0f);
		ImGui::SliderFloat("Radius", &_spotRadius, 0.5f, 25.0f);
		ImGui::SliderFloat("Cone (deg)", &_spotConeDeg, 5.0f, 60.0f);
		break;

	case SelEntity::Post:                                               // T6/T7/T8/T9/T10/T12/T16
		ImGui::SeparatorText("Tonemap / Exposure");
		ImGui::Combo("Operator", &_tonemapOp, "ACES\0Reinhard\0Filmic\0");
		ImGui::SliderFloat("Exposure", &_exposure, 0.1f, 4.0f);
		ImGui::Checkbox("Auto Exposure", &_autoExp);
		if (_autoExp) { ImGui::SliderFloat("Target", &_expTarget, 0.1f, 1.5f); ImGui::SameLine(); ImGui::TextDisabled("(x%.2f)", _expScale); }
		ImGui::SeparatorText("Depth of Field / God Rays");
		ImGui::Checkbox("DOF", &_dofOn);
		ImGui::SliderFloat("Focus Dist", &_dofFocus, 1.0f, 30.0f);
		ImGui::SliderFloat("Focus Range", &_dofRange, 0.5f, 15.0f);
		ImGui::Checkbox("Volumetric Rays", &_volOn);
		ImGui::SliderFloat("Ray Strength", &_volStrength, 0.0f, 2.0f);
		ImGui::SeparatorText("Bloom");
		ImGui::Checkbox("Bloom", &_bloomOn);
		ImGui::SliderFloat("Threshold", &_bloomThreshold, 0.2f, 3.0f);
		ImGui::SliderFloat("Bloom Intensity", &_bloomIntensity, 0.0f, 2.0f);
		ImGui::SeparatorText("Color Grading");
		ImGui::SliderFloat("Contrast", &_contrast, 0.5f, 2.0f);
		ImGui::SliderFloat("Saturation", &_saturation, 0.0f, 2.0f);
		ImGui::SliderFloat("Temperature", &_temperature, -1.0f, 1.0f);
		ImGui::SliderFloat("Vignette", &_vignette, 0.0f, 1.0f);
		ImGui::SeparatorText("Fog / AA / Reflection");
		ImGui::ColorEdit3("Fog Color", &_fogColor.x);
		ImGui::SliderFloat("Fog Density", &_fogDensity, 0.0f, 0.08f);
		ImGui::Checkbox("FXAA", &_fxaaOn);
		ImGui::Checkbox("RT Reflection", &_reflectOn);
		ImGui::SliderFloat("Reflect Strength", &_reflectStrength, 0.0f, 1.0f);
		ImGui::SeparatorText("Ambient Occlusion (RT)");
		ImGui::Checkbox("RT AO", &_aoOn);
		ImGui::SliderFloat("AO Intensity", &_aoIntensity, 0.0f, 2.0f);
		ImGui::SliderFloat("AO Radius", &_aoRadius, 0.1f, 2.0f);
		ImGui::SeparatorText("Lens FX");
		ImGui::SliderFloat("Chromatic Aberr.", &_chroma, 0.0f, 0.02f);
		ImGui::SliderFloat("Film Grain", &_grain, 0.0f, 0.2f);
		ImGui::SliderFloat("Sharpen", &_sharpen, 0.0f, 1.5f);
		ImGui::SliderFloat("Lens Distort", &_lensDistort, -0.5f, 0.5f);   // V10
		ImGui::SliderFloat("Posterize", &_posterize, 0.0f, 16.0f);       // V11 (0=off)
		ImGui::Combo("Filter", &_filterMode, "None\0Sepia\0Grayscale\0Invert\0"); // V12
		ImGui::Checkbox("Anamorphic Bloom", &_anamorphic);               // V19
		ImGui::SliderFloat("Render Scale", &_renderScale, 0.5f, 2.0f);
		ImGui::SeparatorText("Grid / Background");
		ImGui::SliderFloat("Grid Cell", &_gridCell, 0.25f, 5.0f);        // V15
		ImGui::SliderFloat("Grid Fade", &_gridFade, 10.0f, 150.0f);
		ImGui::Combo("Background", &_bgMode, "Sky\0Solid Color\0");      // V17
		if (_bgMode == 1) ImGui::ColorEdit3("BG Color", &_bgColor.x);
		ImGui::SeparatorText("Effects");                                // W1/W4/W5
		ImGui::Checkbox("Particles", &_particlesOn); ImGui::SameLine(); ImGui::Combo("##pm", &_particleMode, "Sparks\0Snow\0");
		ImGui::SliderFloat("Letterbox", &_letterbox, 0.0f, 0.4f);
		ImGui::Checkbox("Composition Overlay", &_overlay);
		if (ImGui::Button("Reset All To Defaults")) ResetDefaults();    // V18
		ImGui::SeparatorText("Debug View / Gizmos");
		ImGui::Combo("View", &_debugView, "Lit\0Albedo\0Normal\0Depth\0GI\0");
		ImGui::Checkbox("Wireframe", &_wireframe); ImGui::SameLine(); ImGui::Checkbox("Frustum Cull", &_frustumCull);
		ImGui::Checkbox("Show Bones", &_showBones); ImGui::SameLine(); ImGui::Checkbox("AABB", &_showAABB);
		ImGui::Checkbox("Light Icons", &_showLightIcons); ImGui::SameLine(); ImGui::Checkbox("Spot Cone", &_showSpotCone);
		ImGui::SeparatorText("Frame Time");
		ImGui::PlotLines("ms", _frameTimes, 120, _frameIdx % 120, nullptr, 0.0f, 40.0f, ImVec2(0, 60));
		ImGui::Text("%.2f ms  /  %.0f FPS", _frameTimes[(_frameIdx + 119) % 120], ImGui::GetIO().Framerate);
		break;

	case SelEntity::Model:
	{
		ImGui::SeparatorText(("Model: " + WToUtf8(_scene._modelLabel)).c_str());
		ImGui::TextDisabled("Gizmo:");                                  // T2
		ImGui::SameLine(); if (ImGui::RadioButton("Move", _gizmoOp == 7)) _gizmoOp = 7;
		ImGui::SameLine(); if (ImGui::RadioButton("Rotate", _gizmoOp == 120)) _gizmoOp = 120;
		ImGui::SameLine(); if (ImGui::RadioButton("Scale", _gizmoOp == 896)) _gizmoOp = 896;
		ImGui::Checkbox("Local", &_gizmoLocal); ImGui::SameLine(); ImGui::Checkbox("Snap", &_snapOn);
		if (ImGui::Button("Reset Transform")) { DirectX::XMStoreFloat4x4(&_scene._modelMatrix, DirectX::XMMatrixIdentity()); }
		// 트랜스폼 수치 입력 (T3)
		{
			float t[3], r[3], sc[3];
			ImGuizmo::DecomposeMatrixToComponents((float*)&_scene._modelMatrix, t, r, sc);
			bool ch = false;
			ch |= ImGui::DragFloat3("Position", t, 0.02f);
			ch |= ImGui::DragFloat3("Rotation", r, 0.5f);
			ch |= ImGui::DragFloat3("Scale", sc, 0.01f);
			if (ch) ImGuizmo::RecomposeMatrixFromComponents(t, r, sc, (float*)&_scene._modelMatrix);
		}
		ImGui::SeparatorText("Material (PBR)");                         // T4/T5
		if (ImGui::Button("Default")) { _matMetallic = 0; _matRoughness = 0.5f; _matEmissive = 0; _matTint = 1; _diffuseTint = { 1,1,1 }; }
		ImGui::SameLine(); if (ImGui::Button("Gold")) { _matMetallic = 1; _matRoughness = 0.25f; _diffuseTint = { 1.0f, 0.78f, 0.34f }; }
		ImGui::SameLine(); if (ImGui::Button("Chrome")) { _matMetallic = 1; _matRoughness = 0.08f; _diffuseTint = { 1,1,1 }; }
		ImGui::SameLine(); if (ImGui::Button("Plastic")) { _matMetallic = 0; _matRoughness = 0.35f; _diffuseTint = { 1,1,1 }; }
		ImGui::ColorEdit3("Diffuse Tint", &_diffuseTint.x);
		ImGui::SliderFloat("Metallic", &_matMetallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &_matRoughness, 0.02f, 1.0f);
		ImGui::SliderFloat("Emissive", &_matEmissive, 0.0f, 5.0f);
		ImGui::SliderFloat("Brightness", &_matTint, 0.0f, 2.0f);
		ImGui::SliderFloat("Normal Map", &_normalIntensity, 0.0f, 3.0f); // V9
		ImGui::SeparatorText("Shading / Outline");
		ImGui::SliderInt("Toon Levels", &_toonLevels, 0, 6);             // V2 (0=off)
		ImGui::SliderFloat("Rim Power", &_rimPower, 0.0f, 8.0f);         // V3
		ImGui::ColorEdit3("Rim Color", &_rimColor.x);
		ImGui::ColorEdit3("Outline Color", &_outlineColor.x);           // V5
		ImGui::SliderFloat("Outline Width", &_outlineThick, 0.0f, 0.03f);
		// 애니메이션 (T17/T18)
		if (!_scene._clips.empty())
		{
			ImGui::SeparatorText("Animation");
			ImGui::Checkbox("Pause", &_animPaused);
			ImGui::SliderFloat("Speed", &_animSpeed, 0.0f, 3.0f);
			std::string clipNames; for (auto& c : _scene._clips) { clipNames += WToUtf8(fs::path(c).stem().wstring()); clipNames.push_back('\0'); }
			if (ImGui::Combo("Clip", &_scene._curClip, clipNames.c_str()))
			{ LoadClip(_scene._clips[_scene._curClip], _scene._clip); _scene._animated = _scene._clip.frameCount > 0; _animTimeAcc = 0.0f; }
		}
		ImGui::SeparatorText("Turntable / History");                   // U14 / U17
		ImGui::Checkbox("Auto-rotate", &_turntable); ImGui::SameLine(); ImGui::SliderFloat("Rot Speed", &_turnSpeed, 0.05f, 2.0f);
		if (ImGui::Button("Checkpoint")) PushUndo();
		ImGui::SameLine(); if (ImGui::Button("Undo")) DoUndo();
		ImGui::SameLine(); if (ImGui::Button("Redo")) DoRedo();
		if (ImGui::Button("Snap To Ground")) // W10
		{
			float t[3], r[3], sc[3]; ImGuizmo::DecomposeMatrixToComponents((float*)&_scene._modelMatrix, t, r, sc);
			t[1] -= _scene._modelMin.y; ImGuizmo::RecomposeMatrixFromComponents(t, r, sc, (float*)&_scene._modelMatrix);
		}
		ImGui::Checkbox("Show Bones", &_showBones); ImGui::SameLine(); ImGui::Checkbox("Show AABB", &_showAABB);
		ImGui::SeparatorText("Info");
		ImGui::Text("Verts %u  Tris %u  Bones %u", _scene._vertexCount, _scene._indexCount / 3, (unsigned)_scene._bonesData.size());
		ImGui::Text("Submeshes %u  Materials %u", (unsigned)_scene._submeshes.size(), _scene._matCount);
		break;
	}

	case SelEntity::Floor:
		ImGui::SeparatorText("Floor / Ground");                        // U9 / U19
		ImGui::ColorEdit3("Color", &_floorColor.x);
		ImGui::SliderFloat("Metallic", &_floorMetallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &_floorRough, 0.02f, 1.0f);
		ImGui::Checkbox("Checker Pattern", &_checker);                  // V16
		if (ImGui::Checkbox("Terrain (heightmap)", &_scene._terrain)) _wantReload = true; // V1
		if (ImGui::SliderFloat("Ground Size", &_scene._groundSize, 3.0f, 20.0f)) _wantReload = true; // V19
		ImGui::SeparatorText("Decal");                                 // W2
		ImGui::Checkbox("Decal", &_decalOn);
		ImGui::DragFloat3("Decal Pos", &_decalPos.x, 0.05f);
		ImGui::ColorEdit3("Decal Color", &_decalColor.x);
		ImGui::SliderFloat("Decal Radius", &_decalRadius, 0.2f, 5.0f);
		break;
	}

	// 선택된 에셋 정보 (FolderContents 에서 클릭)
	if (!_selectedAsset.empty())
	{
		ImGui::Separator();
		ImGui::SeparatorText("Selected Asset");
		fs::path p(_selectedAsset);
		ImGui::Text("Name: %s", WToUtf8(p.filename().wstring()).c_str());
		ImGui::Text("Type: %s", WToUtf8(p.extension().wstring()).c_str());
		std::error_code ec; auto sz = fs::file_size(p, ec);
		if (!ec) ImGui::Text("Size: %.1f KB", sz / 1024.0);
	}

	ImGui::End();
}

// Project — 폴더 트리 (EditorTool 의 Project 창). 폴더 클릭 → FolderContents 가 그 내용 표시
void D3D12Device::DrawProject()
{
	ImGui::Begin("Project");
	std::error_code ec;
	std::function<void(const fs::path&)> rec = [&](const fs::path& dir)
	{
		for (auto& e : fs::directory_iterator(dir, ec))
		{
			if (!e.is_directory(ec)) continue;
			std::string nm = WToUtf8(e.path().filename().wstring());
			ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (e.path() == fs::path(_curDir)) f |= ImGuiTreeNodeFlags_Selected;
			bool open = ImGui::TreeNodeEx(("[/] " + nm).c_str(), f);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) _curDir = e.path().wstring();
			if (open) { rec(e.path()); ImGui::TreePop(); }
		}
	};
	if (ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow))
	{
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) _curDir = _assetRoot;
		rec(fs::path(_assetRoot)); ImGui::TreePop();
	}
	ImGui::End();
}

// GameObject 컴포넌트 기반 인스펙터 (EditorTool ShowInfoHiearchy 대응) —
// 고정 컴포넌트 순회 + EnumToString 헤더 + 각 컴포넌트 OnInspectorGUI + 스크립트.
void D3D12Device::DrawGameObjectInspector(const shared_ptr<GameObject>& go)
{
	// 활성 토글 + 이름 편집 (Rename → Scene 이름 캐시 재등록)
	bool active = go->IsActive();
	if (ImGui::Checkbox("##active", &active)) go->SetActive(active);
	ImGui::SameLine();
	char nameBuf[128]; std::string nm = WToUtf8(go->GetObjectName());
	strncpy_s(nameBuf, nm.c_str(), sizeof(nameBuf) - 1);
	ImGui::SetNextItemWidth(-1);
	if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue) && nameBuf[0])
	{
		go->SetObjectName(Utf8ToW(nameBuf));
		if (_gameScene) _gameScene->RegisterName(go);
	}
	ImGui::TextDisabled("ID %lld%s", (long long)go->GetId(), go->IsEditorInternal() ? "  (editor)" : "");
	ImGui::Separator();

	const bool internal = go->IsEditorInternal();
	for (uint8 i = 0; i < static_cast<uint8>(ComponentType::End); ++i)
	{
		ComponentType ct = static_cast<ComponentType>(i);
		auto c = go->GetFixedComponent(ct);
		if (!c) continue;

		if (ct == ComponentType::Renderer)
		{
			if (auto r = go->GetRenderer())
				ImGui::SeparatorText(ImGuiManager::EnumToString(r->GetRenderType()));
		}
		else
			ImGui::SeparatorText(ImGuiManager::EnumToString(ct));

		// 컴포넌트 제거 (Transform/에디터내부/메인모델 렌더러 제외)
		bool canRemove = ct != ComponentType::Transform && !internal && !(go == _modelObj && ct == ComponentType::Renderer);
		if (canRemove)
		{
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 18.0f);
			ImGui::PushID(i);
			if (ImGui::SmallButton("X")) { go->RemoveComponent(ct); ImGui::PopID(); continue; }
			ImGui::PopID();
		}

		c->OnInspectorGUI();
	}

	// ── Terrain 편집 패널 (Terrain 컴포넌트 보유 시) ──
	if (auto terr = go->GetComponent<Terrain>())
	{
		ImGui::SeparatorText("Terrain Sculpt");
		ImGui::Checkbox("Edit Mode (좌드래그=스컬프트)", &_terrainEdit);
		const char* brushes[] = { "Raise", "Lower", "Smooth", "Flatten", "Paint" };
		ImGui::Combo("Brush", &_terrainBrush, brushes, 5);
		ImGui::DragFloat("Radius", &_terrainRadius, 0.2f, 0.5f, terr->WorldSize() * 0.5f);
		ImGui::DragFloat("Strength", &_terrainStrength, 0.2f, 0.1f, 50.f);
		if (_terrainBrush == 3) ImGui::DragFloat("Flatten Height", &_terrainFlatten, 0.1f, -50.f, 50.f);
		if (_terrainBrush == 4) ImGui::ColorEdit3("Paint Color", &_terrainPaintColor.x);
		ImGui::TextDisabled("Grid %dx%d  cell %.2fm  size %.0fm", terr->GridN(), terr->GridN(), terr->CellSize(), terr->WorldSize());

		if (ImGui::Button("Save Heightmap..."))
		{
			wchar_t file[MAX_PATH] = L"terrain_edit.r32";
			OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
			ofn.lpstrFilter = L"Heightmap (*.r32)\0*.r32\0"; ofn.lpstrDefExt = L"r32";
			ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
			if (GetSaveFileNameW(&ofn)) { if (terr->SaveHeightmap(file)) Log("Heightmap saved: " + WToUtf8(file)); }
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Heightmap..."))
		{
			wchar_t file[MAX_PATH] = L"";
			OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
			ofn.lpstrFilter = L"Heightmap (*.r32)\0*.r32\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
			if (GetOpenFileNameW(&ofn)) { if (terr->LoadHeightmap(file)) Log("Heightmap loaded: " + WToUtf8(file)); }
		}
	}

	{
		shared_ptr<MonoBehaviour> removeScript;
		int si = 0;
		for (auto& s : go->GetMonoBehaviours())
		{
			if (!s) continue;
			ImGui::SeparatorText(s->TypeName());
			if (!internal)
			{
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 18.0f);
				ImGui::PushID(1000 + si);
				if (ImGui::SmallButton("X")) removeScript = s;
				ImGui::PopID();
			}
			s->OnInspectorGUI();
			++si;
		}
		if (removeScript) go->RemoveScript(removeScript);
	}

	// ── Add Component (에디터 내부 오브젝트 제외) ──
	if (!internal)
	{
		ImGui::Separator();
		if (ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("addcomp");
		if (ImGui::BeginPopup("addcomp"))
		{
			if (!go->GetRenderer() && ImGui::MenuItem("Mesh Renderer"))
			{
				auto mr = make_shared<MeshRenderer>(); mr->Bind(this);
				vector<Vtx> v; vector<uint32> idx; BuildPrim(MeshPrim::Cube, v, idx);
				mr->SetGeometry(v, idx); mr->SetPrim(MeshPrim::Cube);
				go->AddComponent(mr);
				Log("Added MeshRenderer to " + WToUtf8(go->GetObjectName()));
			}
			if (!go->GetRenderer() && ImGui::MenuItem("Model Animator"))
			{
				auto an = make_shared<ModelAnimator>(); an->Bind(this);
				if (an->Load(_assetRoot + L"\\Models\\Archer\\Archer.mesh"))
				{ go->AddComponent(an); Log("Added ModelAnimator to " + WToUtf8(go->GetObjectName())); }
				else Log("ModelAnimator load FAILED");
			}
			if (!go->GetLight() && ImGui::MenuItem("Light"))
			{
				auto l = make_shared<Light>();
				l->_lightType = LightType::Point; l->_color = Vec3{ 1.f, 0.8f, 0.6f }; l->_intensity = 3.f; l->_range = 6.f;
				go->AddComponent(l);
				Log("Added Light to " + WToUtf8(go->GetObjectName()));
			}
			if (!go->GetRenderer() && ImGui::MenuItem("Particle System"))
			{ auto ps = make_shared<ParticleSystem>(); go->AddComponent(ps); Log("Added ParticleSystem to " + WToUtf8(go->GetObjectName())); }
			if (!go->GetComponent<BaseCollider>() && ImGui::MenuItem("Box Collider"))
			{ go->AddComponent(make_shared<AABBBoxCollider>()); Log("Added Box Collider to " + WToUtf8(go->GetObjectName())); }
			if (!go->GetComponent<BaseCollider>() && ImGui::MenuItem("Sphere Collider"))
			{ go->AddComponent(make_shared<SphereCollider>()); Log("Added Sphere Collider to " + WToUtf8(go->GetObjectName())); }
			if (ImGui::BeginMenu("Script"))
			{
				for (auto& kv : ScriptRegistry::Map())
					if (ImGui::MenuItem(kv.first.c_str()))
					{ go->AddComponent(ScriptRegistry::Create(kv.first)); Log("Added script: " + kv.first); }
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
	}
}

// 확장자 → 타입 분류 (아이콘 색/태그용)
namespace
{
	enum class AssetKind { Folder, Mesh, Material, Image, Clip, Scene, Other };

	AssetKind ClassifyExt(const std::wstring& ext)
	{
		if (ext == L".mesh") return AssetKind::Mesh;
		if (ext == L".mat" || ext == L".mmat") return AssetKind::Material;
		if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".dds" || ext == L".tga" || ext == L".bmp") return AssetKind::Image;
		if (ext == L".clip") return AssetKind::Clip;
		if (ext == L".scene" || ext == L".rtscene") return AssetKind::Scene;
		return AssetKind::Other;
	}

	// 타입별 아이콘 색 + 짧은 태그
	void KindStyle(AssetKind k, ImVec4& col, const char*& tag)
	{
		switch (k)
		{
		case AssetKind::Folder:   col = ImVec4(0.86f, 0.72f, 0.30f, 1.0f); tag = "DIR";  break;
		case AssetKind::Material: col = ImVec4(0.30f, 0.62f, 0.46f, 1.0f); tag = "MAT";  break;
		case AssetKind::Image:    col = ImVec4(0.36f, 0.52f, 0.80f, 1.0f); tag = "IMG";  break;
		case AssetKind::Clip:     col = ImVec4(0.66f, 0.42f, 0.74f, 1.0f); tag = "CLIP"; break;
		case AssetKind::Scene:    col = ImVec4(0.74f, 0.46f, 0.30f, 1.0f); tag = "SCN";  break;
		default:                  col = ImVec4(0.34f, 0.34f, 0.38f, 1.0f); tag = "FILE"; break;
		}
	}

	// 파일명을 셀 폭에 맞게 줄임 (...)
	std::string FitName(const std::string& s, float maxW)
	{
		if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
		std::string out = s;
		while (out.size() > 4 && ImGui::CalcTextSize((out + "..").c_str()).x > maxW)
			out.pop_back();
		return out + "..";
	}
}

void D3D12Device::DrawFolderContents()
{
	ImGui::Begin("FolderContents");

	// ── 경로 바 + 상위 폴더 ──
	bool atRoot = (fs::path(_curDir) == fs::path(_assetRoot));
	ImGui::BeginDisabled(atRoot);
	if (ImGui::Button("  ..  ") && !atRoot) _curDir = fs::path(_curDir).parent_path().wstring();
	ImGui::EndDisabled();
	ImGui::SameLine();
	std::wstring rel = fs::relative(_curDir, fs::path(_assetRoot).parent_path()).wstring();
	ImGui::TextDisabled("%s", WToUtf8(rel).c_str());
	ImGui::Separator();

	// ── 항목 수집 (폴더 먼저, 이름순) ──
	std::error_code ec;
	std::vector<fs::path> dirs, files;
	for (auto& e : fs::directory_iterator(_curDir, ec))
	{
		if (e.is_directory(ec)) dirs.push_back(e.path());
		else                    files.push_back(e.path());
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());

	// ── 썸네일 그리드 (EditorTool FolderContents 스타일) ──
	const float kIcon = 64.0f;
	const float kCell = 84.0f; // 아이콘 + 좌우 여백
	float availW = ImGui::GetContentRegionAvail().x;
	int columns = (std::max)(1, (int)(availW / kCell));

	ImGuiStyle& st = ImGui::GetStyle();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.42f, 0.69f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.56f, 0.96f, 1.0f));

	if (ImGui::BeginTable("FolderGrid", columns, ImGuiTableFlags_NoBordersInBody))
	{
		auto drawCell = [&](const fs::path& p, AssetKind kind, bool isDir)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(WToUtf8(p.wstring()).c_str());

			float cellW = ImGui::GetColumnWidth();
			float padX = (cellW - kIcon) * 0.5f;
			if (padX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padX);

			bool clicked = false, dbl = false;
			const std::wstring full = p.wstring();
			const bool selected = (full == _selectedAsset);

			// 선택 항목 테두리 강조
			if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.34f, 0.55f, 1.0f));

			uint64 thumb = 0;
			if (kind == AssetKind::Mesh)       thumb = _thumbnail.GetMesh(full);
			else if (kind == AssetKind::Image) thumb = _thumbnail.GetImage(full);

			if (thumb)
			{
				clicked = ImGui::ImageButton("##th", (ImTextureID)thumb, ImVec2(kIcon, kIcon));
			}
			else
			{
				// 아이콘 버튼 (타입색 + 태그) — 메시 썸네일 생성 대기 중에도 동일 형태
				ImVec4 col; const char* tag = "FILE";
				KindStyle(kind, col, tag);
				ImGui::PushStyleColor(ImGuiCol_Button, col);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 1.25f, col.y * 1.25f, col.z * 1.25f, 1.0f));
				clicked = ImGui::Button(tag, ImVec2(kIcon, kIcon));
				ImGui::PopStyleColor(2);
			}
			if (selected) ImGui::PopStyleColor();

			// .mesh 드래그 → Scene 뷰에 드롭하면 배치
			if (!isDir && kind == AssetKind::Mesh && ImGui::BeginDragDropSource())
			{
				std::string up = WToUtf8(full);
				ImGui::SetDragDropPayload("MESH_PATH", up.c_str(), up.size() + 1);
				ImGui::TextUnformatted(WToUtf8(p.filename().wstring()).c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) dbl = true;

			if (clicked && !isDir) _selectedAsset = full;
			if (dbl)
			{
				if (isDir) _curDir = full;
				else if (kind == AssetKind::Mesh) { _selectedAsset = full; Vec3 sp = SpawnPoint(); SpawnAnimatedModel(full, Vec3{ sp.x, 0, sp.z }); } // 씬에 추가 배치(정적/애니 공통)
				else if (kind == AssetKind::Scene) { _selectedAsset = full; } // (씬 로드는 메뉴 Open Scene)
				else if (kind == AssetKind::Material && p.extension() == L".mat") // .mat → 선택 메시에 공유 적용
				{
					_selectedAsset = full;
					if (_selectedGO) if (auto mr = _selectedGO->GetMeshRenderer())
					{
						auto shared = GET_SINGLE(ResourceManager)->Get<Material>(full);
						if (!shared) { shared = LoadMaterial(full); if (shared) GET_SINGLE(ResourceManager)->Add<Material>(full, shared); }
						if (shared) { mr->SetMaterialRef(shared); Log("Assigned material: " + WToUtf8(p.filename().wstring())); }
					}
				}
			}

			// 중앙정렬 파일명
			std::string nm = FitName(WToUtf8(p.filename().wstring()), cellW - 4.0f);
			float tw = ImGui::CalcTextSize(nm.c_str()).x;
			float tpad = (cellW - tw) * 0.5f;
			if (tpad > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + tpad);
			ImGui::TextUnformatted(nm.c_str());

			ImGui::PopID();
		};

		for (auto& d : dirs)  drawCell(d, AssetKind::Folder, true);
		for (auto& f : files) drawCell(f, ClassifyExt(f.extension().wstring()), false);

		ImGui::EndTable();
	}

	ImGui::PopStyleColor(3);

	ImGui::Separator();
	ImGui::TextDisabled("Double-click a folder to open, a [mesh] to load");

	ImGui::End();
}

// ───────────────────────────────────────────────────────────
// 에디터 윈도우 (EditorTool 패턴) — 각 패널은 D3D12Device 의 해당 Draw* 를 렌더한다.
// EditorManager 가 등록·순회하고, 각 윈도우가 자기 ImGui::Begin/End 를 담당.
// ───────────────────────────────────────────────────────────
void MainMenuBarWindow::Update()   { _dev->DrawMainMenuBar(); }
void SceneViewWindow::Update()     { _dev->DrawSceneView(); }
void HierarchyWindow::Update()     { _dev->DrawHierarchy(); }
void InspectorWindow::Update()     { _dev->DrawInspector(); }
void ProjectWindow::Update()       { _dev->DrawProject(); }
void FolderContentsWindow::Update(){ _dev->DrawFolderContents(); }
void LogPanelWindow::Update()      { _dev->DrawLog(); }
