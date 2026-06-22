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
#include "EditorUtil.h"   // WToUtf8 / BuildPrim (Spawn/Serialize/Inspector 와 공유)

namespace fs = std::filesystem;

// Utf8ToW 는 MeshLoader.h 에 정의됨 (재사용)

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
			// 선택된 라이트면 영향 반경(range) 와이어 구 표시 (어둡게)
			if (_selectedGO == o && l->_lightType == LightType::Point && l->_range > 0.f)
			{
				XMFLOAT3 dim{ col.x * 0.5f, col.y * 0.5f, col.z * 0.5f };
				wireCircle(c, XMVectorSet(1,0,0,0), XMVectorSet(0,1,0,0), l->_range, dim);
				wireCircle(c, XMVectorSet(1,0,0,0), XMVectorSet(0,0,1,0), l->_range, dim);
				wireCircle(c, XMVectorSet(0,1,0,0), XMVectorSet(0,0,1,0), l->_range, dim);
			}
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

	// ── 월드 AABB / 스켈레톤 본 (씬그래프 GameObject 단위 — 모델 인스펙터에서 이전된 디버그 뷰) ──
	if ((_showAABB || _showBones) && _gameScene)
		for (auto& kv : _gameScene->GetCreatedObjects())
		{
			auto& o = kv.second; if (!o || !o->IsActive() || o->IsEditorInternal()) continue;
			auto r = o->GetRenderer(); if (!r) continue;
			if (_showAABB) // 렌더러 월드 바운딩 박스(스키닝/트랜스폼 반영, SortGameObject 갱신)
			{
				const DirectX::BoundingBox& bb = r->GetBoundingBox();
				XMFLOAT3 c = bb.Center, e = bb.Extents;
				XMFLOAT3 p[8] = {
					{c.x-e.x,c.y-e.y,c.z-e.z},{c.x+e.x,c.y-e.y,c.z-e.z},{c.x+e.x,c.y-e.y,c.z+e.z},{c.x-e.x,c.y-e.y,c.z+e.z},
					{c.x-e.x,c.y+e.y,c.z-e.z},{c.x+e.x,c.y+e.y,c.z-e.z},{c.x+e.x,c.y+e.y,c.z+e.z},{c.x-e.x,c.y+e.y,c.z+e.z} };
				int e8[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
				const XMFLOAT3 cy{ 0.2f, 0.85f, 1.0f };
				for (int k = 0; k < 24; k += 2) _debugDraw.Line(p[e8[k]], p[e8[k + 1]], cy);
			}
			if (_showBones) // 애니메이터 현재 포즈 스켈레톤 (부모→자식 선분 + 관절 크로스)
				if (auto anm = std::dynamic_pointer_cast<ModelAnimator>(r))
				{
					std::vector<std::pair<XMFLOAT3, XMFLOAT3>> lines;
					anm->GetBoneLines(lines);
					const XMFLOAT3 bc{ 1.0f, 0.45f, 0.1f };
					// overlay=true → 깊이 무시(메시 관통) — 안 하면 본이 메시 안에 가려 안 보임
					for (auto& ln : lines) { _debugDraw.Line(ln.first, ln.second, bc, true); _debugDraw.Cross(ln.second, bc, 0.025f, true); }
				}
		}

	// ParticleSystem 컴포넌트 입자는 GPU 인스턴스드 빌보드(RenderParticles)로 렌더 — 여기선 생략.

	// ── 디렉셔널 라이트(태양) 방향 화살표 (씬 원점 위, 노랑) ──
	if (_showLightIcons)
	{
		auto sunL = _sunObj ? _sunObj->GetLight() : nullptr;
		XMVECTOR dir = sunL ? XMLoadFloat3(&sunL->_direction)
		                    : XMVectorSet(cosf(_lightAngle) * 0.6f, -1.f, sinf(_lightAngle) * 0.6f, 0.f);
		if (XMVectorGetX(XMVector3LengthSq(dir)) > 1e-6f)
		{
			dir = XMVector3Normalize(dir);
			XMVECTOR origin = XMVectorSet(0, 5.f, 0, 0);          // 화살표 시작(씬 위)
			XMVECTOR tip = XMVectorAdd(origin, XMVectorScale(dir, 2.5f));
			const XMFLOAT3 yc{ 1.0f, 0.92f, 0.25f };
			XMFLOAT3 o, t; XMStoreFloat3(&o, origin); XMStoreFloat3(&t, tip);
			_debugDraw.Line(o, t, yc);
			// 화살촉 (dir 에 수직한 두 벡터로)
			XMVECTOR up = fabsf(XMVectorGetY(dir)) > 0.95f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
			XMVECTOR rt = XMVector3Normalize(XMVector3Cross(up, dir));
			XMVECTOR bt = XMVector3Cross(dir, rt);
			XMVECTOR back = XMVectorAdd(tip, XMVectorScale(dir, -0.5f));
			for (int s = 0; s < 4; ++s)
			{
				XMVECTOR off = (s == 0) ? rt : (s == 1) ? XMVectorNegate(rt) : (s == 2) ? bt : XMVectorNegate(bt);
				XMVECTOR p = XMVectorAdd(back, XMVectorScale(off, 0.18f));
				XMFLOAT3 pf; XMStoreFloat3(&pf, p);
				_debugDraw.Line(t, pf, yc);
			}
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
	Log("Reset scene to defaults");
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

// UI 스케일 — 기본 스타일을 보존해 두고 스케일 변경 시 항상 기준에서 다시 적용(누적 방지).
static ImGuiStyle g_baseStyle;
static float      g_uiScale = 1.0f;
static void ApplyUIScale(float s)
{
	s = s < 0.5f ? 0.5f : (s > 3.0f ? 3.0f : s); // 방어적 클램프 (폰트/위젯 깨짐 방지)
	g_uiScale = s;
	ImGuiStyle& st = ImGui::GetStyle();
	st = g_baseStyle;            // 기준 스타일 복원
	st.ScaleAllSizes(s);        // 패딩/라운딩/스크롤바 등 전부 스케일
	ImGui::GetIO().FontGlobalScale = s; // 폰트 스케일
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
	g_baseStyle = ImGui::GetStyle(); // UI 스케일 기준 보존

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
	LoadSkyCubemap(_assetRoot + L"\\Textures\\desertcube1024.dds"); // 큐브맵 사전 로드(토글 OFF 기본)
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
	_editor.Update(); // 내부에서 ImGui::Render() 호출 — HUD 는 그 직전에 EditorManager 가 DrawGameHUD() 호출
}

// 게임 HUD (Play 중 — 플레이어 HP / 적 수). EditorManager::Update 가 ImGui::Render() 직전에 호출(프레임 스코프 내).
void D3D12Device::DrawGameHUD()
{
	if (!_playing || !_gameScene) return;
	float php = -1.f, pmax = 100.f; int alive = 0, total = 0;
	for (auto& kv : _gameScene->GetCreatedObjects())
	{
		auto& o = kv.second; if (!o) continue;
		if (auto pc = o->GetComponent<PlayerController>()) { php = pc->_hp; pmax = pc->_maxHp; }
		if (auto ec = o->GetComponent<EnemyController>()) { ++total; if (!ec->_dead && o->IsActive()) ++alive; }
	}
	if (php < 0.f) return;
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f - 120.f, vp->WorkPos.y + 90.f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.4f);
	ImGui::Begin("##HUD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
	if (php <= 0.f) ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "YOU DIED");
	else
	{
		ImGui::TextUnformatted("HP"); ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.85f, 0.22f, 0.22f, 1));
		ImGui::ProgressBar(php / (pmax > 0 ? pmax : 1.f), ImVec2(200, 16));
		ImGui::PopStyleColor();
	}
	if (total > 0 && alive == 0) ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "CLEAR!  (%d/%d)", total, total);
	else ImGui::Text("Enemies: %d / %d", alive, total);
	ImGui::End();
}

// 메인 메뉴바 (EditorTool 구조: File / Edit / GameObject / Component / View) — MainMenuBarWindow 가 호출
void D3D12Device::DrawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N")) NewScene();
			if (ImGui::MenuItem("Open Scene (quick)", "Ctrl+O")) LoadScene();
			if (ImGui::MenuItem("Save Scene (quick)", "Ctrl+S")) SaveScene();
			if (ImGui::MenuItem("Open Scene As..."))
			{
				wchar_t file[MAX_PATH] = L"";
				OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
				ofn.lpstrFilter = L"Scene (*.rtscene)\0*.rtscene\0All\0*.*\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
				ofn.lpstrInitialDir = nullptr; ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
				std::wstring dir = _assetRoot + L"\\Scenes"; ofn.lpstrInitialDir = dir.c_str();
				if (GetOpenFileNameW(&ofn)) LoadSceneFrom(file);
			}
			if (ImGui::MenuItem("Save Scene As..."))
			{
				wchar_t file[MAX_PATH] = L"untitled.rtscene";
				OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = _hwnd;
				ofn.lpstrFilter = L"Scene (*.rtscene)\0*.rtscene\0"; ofn.lpstrDefExt = L"rtscene";
				ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
				std::wstring dir = _assetRoot + L"\\Scenes"; ofn.lpstrInitialDir = dir.c_str();
				if (GetSaveFileNameW(&ofn)) SaveSceneTo(file);
			}
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
			bool hasSel = _selectedGO && !_selectedGO->IsEditorInternal();
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSel)) DuplicateSelectedObject();
			if (ImGui::MenuItem("Delete", "Del", false, hasSel)) DeleteSelectedObject();
			if (ImGui::MenuItem("Group Selected", "Ctrl+G", false, hasSel)) GroupSelected();
			if (ImGui::MenuItem("Snap Selected to Grid", nullptr, false, hasSel)) SnapSelectedToGrid();
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
				if (ImGui::MenuItem("Cylinder")) { BuildPrim(MeshPrim::Cylinder, v, idx); SpawnMeshObject(L"Cylinder", v, idx, sp, MeshPrim::Cylinder); }
				if (ImGui::MenuItem("Cone"))     { BuildPrim(MeshPrim::Cone, v, idx);     SpawnMeshObject(L"Cone", v, idx, sp, MeshPrim::Cone); }
				if (ImGui::MenuItem("Torus"))    { BuildPrim(MeshPrim::Torus, v, idx);    SpawnMeshObject(L"Torus", v, idx, sp, MeshPrim::Torus); }
				if (ImGui::MenuItem("Capsule"))  { BuildPrim(MeshPrim::Capsule, v, idx);  SpawnMeshObject(L"Capsule", v, idx, sp, MeshPrim::Capsule); }
				ImGui::Separator();
				if (ImGui::MenuItem("Animated Model (Archer)"))
					SpawnAnimatedModel(_assetRoot + L"\\Models\\Archer\\Archer.mesh", Vec3{ sp.x, 0, sp.z });
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Directional Light")) { _sel = SelEntity::Sun; _selectedGO = nullptr; } // 디렉셔널=단일 sun(선택)
			if (ImGui::MenuItem("Point Light"))       SpawnLight(1, L"Point Light", sp);
			if (ImGui::MenuItem("Spot Light"))        SpawnLight(2, L"Spot Light", sp);
			if (ImGui::MenuItem("Point Light x9 (demo)")) // 다중 라이트(최대16) 데모 — 색색 점광원 3x3 격자
			{
				const Vec3 cols[9] = { {1,0.3f,0.3f},{0.3f,1,0.3f},{0.3f,0.5f,1},{1,1,0.3f},{1,0.4f,1},{0.3f,1,1},{1,0.6f,0.2f},{0.6f,0.3f,1},{1,1,1} };
				int k = 0;
				for (int z = -1; z <= 1; ++z) for (int x = -1; x <= 1; ++x, ++k)
				{
					auto o = SpawnLight(1, L"PtGrid", Vec3{ x * 3.0f, 1.2f, z * 3.0f });
					if (o) if (auto l = o->GetLight()) { l->_color = cols[k]; l->_intensity = 3.0f; l->_range = 4.5f; }
				}
			}
			if (ImGui::MenuItem("Particle System"))   { auto o = SpawnEmpty(L"Particles", sp); if (o) o->AddComponent(make_shared<ParticleSystem>()); }
			if (ImGui::MenuItem("Billboard"))         { auto o = SpawnEmpty(L"Billboard", sp); if (o) o->AddComponent(make_shared<Billboard>()); }
			if (ImGui::MenuItem("Camera")) { auto o = SpawnEmpty(L"Camera", _camera.pos); if (o) { o->AddComponent(make_shared<Camera>()); if (auto t = o->GetTransform()) t->SetLocalRotation(Vec3{ _camera.pitch, _camera.yaw, 0.f }); } }
			ImGui::Separator();
			if (ImGui::MenuItem("Terrain")) SpawnTerrain(128, 1.0f);
			ImGui::Separator();
			if (ImGui::MenuItem("Instantiate Prefab...")) InstantiatePrefab();
			if (ImGui::MenuItem("Save Selected as Prefab...", nullptr, false, _selectedGO != nullptr)) SaveSelectedAsPrefab();
			ImGui::Separator();
			if (ImGui::MenuItem("Spawn Showcase Scene")) SpawnShowcase();
			if (ImGui::MenuItem("Spawn Character Showcase")) SpawnCharacterShowcase();
			if (ImGui::MenuItem("Spawn Action Arena")) SpawnActionArena();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Component"))
		{
			if (ImGui::MenuItem("Lighting / Sky")) _sel = SelEntity::Sun;
			if (ImGui::MenuItem("Camera")) _sel = SelEntity::Camera;
			if (ImGui::MenuItem("Post / Render")) _sel = SelEntity::Post;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", nullptr, &_showGrid);
			ImGui::MenuItem("Floor", nullptr, &_showFloor);
			ImGui::MenuItem("Sky", nullptr, &_showSky);
			if (_skyCube) ImGui::MenuItem("Skybox Cubemap", nullptr, &_skyCubemapOn);
			ImGui::MenuItem("Bloom", nullptr, &_bloomOn);
			ImGui::MenuItem("Wireframe", nullptr, &_wireframe);
			ImGui::MenuItem("Terrain Tessellation (GPU)", nullptr, &_tessTerrain);
			if (_tessTerrain) { ImGui::SetNextItemWidth(120); ImGui::SliderFloat("Tess Factor", &_tessFactor, 1.f, 64.f, "%.0f"); }
			ImGui::MenuItem("Water Plane", nullptr, &_waterOn);
			if (_waterOn) { ImGui::SetNextItemWidth(120); ImGui::SliderFloat("Water Level", &_waterLevel, -5.f, 10.f, "%.1f"); }
			ImGui::Separator();
			// UI 스케일 (고해상도 대응 — 전체 폰트/위젯 크기)
			float sc = g_uiScale;
			ImGui::SetNextItemWidth(120);
			if (ImGui::SliderFloat("UI Scale", &sc, 0.8f, 2.0f, "%.2fx")) ApplyUIScale(sc);
			if (ImGui::MenuItem("Reset UI Scale")) ApplyUIScale(1.0f);
			if (ImGui::MenuItem("Reset Layout")) _resetLayout = true; // 다음 프레임 도킹 레이아웃 재구성
			ImGui::EndMenu();
		}
		float fps = ImGui::GetIO().Framerate;
		int objCount = _gameScene ? (int)_gameScene->GetCreatedObjects().size() : 0;
		char stat[160];
		snprintf(stat, sizeof(stat), "%.1f FPS  |  %d objs  |  %u tris  |  %u probes  |  DXR RT", fps, objCount, _scene._indexCount / 3, Ddgi::PROBE_COUNT);
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
	ImGui::TextDisabled("SCENE SETTINGS");
	ImGui::Separator();

	// 인라인 리네임 상태 (더블클릭/F2 진입 → InputText, Enter/포커스아웃 커밋, Esc 취소) — 함수 스코프(트리 람다 + F2 키 공용)
	static int64 s_renameId = -1; static char s_renameBuf[128] = {}; static bool s_renameInit = false;

	// 씬/렌더 전역 설정 (GameObject 가 아닌 싱글톤) — 점/스팟 라이트는 SCENE OBJECTS 에서 편집.
	struct Fixed { const char* tag; ImVec4 col; std::string name; SelEntity e; };
	const Fixed fixed[] = {
		{ "CAM", ImVec4(0.40f,0.78f,0.92f,1), "Editor Camera",   SelEntity::Camera }, // 에디터 뷰포트
		{ "SKY", ImVec4(0.96f,0.80f,0.32f,1), "Lighting / Sky",  SelEntity::Sun },    // 태양+하늘+그림자+앰비언트
		{ "GI",  ImVec4(0.66f,0.52f,0.86f,1), "DDGI Volume",     SelEntity::DDGI },
		{ "FX",  ImVec4(0.42f,0.82f,0.70f,1), "Post / Render",   SelEntity::Post },
		{ "GEO", ImVec4(0.74f,0.58f,0.36f,1), "Floor",           SelEntity::Floor }, // 모델은 SCENE OBJECTS 의 GameObject
	};
	for (const Fixed& f : fixed)
	{
		ImGui::TextColored(f.col, "%s", f.tag); ImGui::SameLine(46.0f);
		ImGui::PushID((int)f.e);
		if (ImGui::Selectable(f.name.c_str(), _sel == f.e && !_selectedGO))
		{ _sel = f.e; _selectedGO = nullptr; _selIds.clear(); } // 고정 엔티티 선택 시 GameObject 선택 해제
		ImGui::PopID();
	}

	// ── 씬 그래프 GameObject 목록 (EditorTool Hierarchy 대응) ──
	ImGui::Spacing();
	ImGui::TextDisabled("SCENE OBJECTS  (drag = reparent)");
	ImGui::Separator();
	static char hierFilter[64] = "";
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##hierfilter", "Search...", hierFilter, sizeof(hierFilter));
	std::string filterLow = hierFilter;
	for (char& c : filterLow) c = (char)tolower(c);
	// GameObject 타입 태그 + 색상 (컴포넌트 기반) — 색상 코딩으로 한눈에 종류 식별
	struct TypeStyle { const char* tag; ImVec4 col; };
	auto typeStyle = [](const shared_ptr<GameObject>& o) -> TypeStyle
	{
		if (o->GetCamera())  return { "CAM", ImVec4(0.40f, 0.78f, 0.92f, 1.f) }; // 시안
		if (o->GetLight())   return { "LIT", ImVec4(0.96f, 0.80f, 0.32f, 1.f) }; // 앰버
		if (o->GetTerrain()) return { "TER", ImVec4(0.74f, 0.58f, 0.36f, 1.f) }; // 갈색
		if (auto r = o->GetRenderer())
		{
			switch (r->GetRenderType())
			{
			case RendererType::Foliage:   return { "FOL", ImVec4(0.48f, 0.78f, 0.42f, 1.f) }; // 초록
			case RendererType::Particle:  return { "PSY", ImVec4(0.72f, 0.52f, 0.88f, 1.f) }; // 보라
			case RendererType::Animator:  return { "ANM", ImVec4(0.52f, 0.82f, 0.66f, 1.f) }; // 청록
			case RendererType::Mesh:      return { "MSH", ImVec4(0.58f, 0.68f, 0.82f, 1.f) }; // 청회
			case RendererType::Billboard: return { "BBD", ImVec4(0.88f, 0.66f, 0.52f, 1.f) }; // 살구
			default: break;
			}
		}
		return { "OBJ", ImVec4(0.62f, 0.62f, 0.68f, 1.f) }; // 회색
	};
	auto drawTag = [](const TypeStyle& ts) { ImGui::TextColored(ts.col, "%s", ts.tag); ImGui::SameLine(0.0f, 6.0f); };
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
			if (_selectedGO == obj || IsMultiSelected(obj->GetId())) flags |= ImGuiTreeNodeFlags_Selected;
			// 행별 활성(가시성) 체크박스
			ImGui::PushID((int)(intptr_t)obj->GetId());
			bool act = obj->IsActive();
			if (ImGui::Checkbox("##vis", &act)) obj->SetActive(act);
			ImGui::PopID();
			ImGui::SameLine(0.0f, 4.0f);
			drawTag(typeStyle(obj)); // 색상 타입 태그
			std::string label = WToUtf8(obj->GetObjectName());
			bool renaming = (s_renameId == obj->GetId());
			bool open = false;
			if (renaming)
			{
				// 트리 펼침 없이 한 줄 이름 편집
				ImGui::SetNextItemWidth(-1.0f);
				if (s_renameInit) { ImGui::SetKeyboardFocusHere(); s_renameInit = false; }
				bool commit = ImGui::InputText("##rename", s_renameBuf, sizeof(s_renameBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
				bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
				if (commit || ImGui::IsItemDeactivated() || esc)
				{
					if (!esc && s_renameBuf[0]) obj->SetObjectName(Utf8ToW(s_renameBuf));
					s_renameId = -1;
				}
			}
			else
			{
				open = ImGui::TreeNodeEx((void*)(intptr_t)obj->GetId(), flags, "%s", label.c_str());
				// 더블클릭(토글 아님) → 리네임 진입
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !ImGui::IsItemToggledOpen())
				{
					s_renameId = obj->GetId(); s_renameInit = true;
					std::string cur = WToUtf8(obj->GetObjectName());
					strncpy_s(s_renameBuf, cur.c_str(), _TRUNCATE);
				}
			}
			if (!renaming && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !ImGui::IsMouseDoubleClicked(0))
			{
				ImGuiIO& gio = ImGui::GetIO();
				if (gio.KeyShift && _anchorId >= 0)
				{
					// 범위 선택 — 생성순(id) 정렬 비-내부 목록에서 anchor~clicked 사이 전부 선택
					std::vector<int64> ord;
					for (auto& kv2 : _gameScene->GetCreatedObjects()) if (kv2.second && !kv2.second->IsEditorInternal()) ord.push_back(kv2.first);
					std::sort(ord.begin(), ord.end());
					int ia = -1, ib = -1;
					for (int k = 0; k < (int)ord.size(); ++k) { if (ord[k] == _anchorId) ia = k; if (ord[k] == obj->GetId()) ib = k; }
					if (ia >= 0 && ib >= 0)
					{
						if (ia > ib) std::swap(ia, ib);
						_selIds.clear();
						for (int k = ia; k <= ib; ++k) if (ord[k] != obj->GetId()) _selIds.push_back(ord[k]);
						_selectedGO = obj; // primary = 마지막 클릭
					}
				}
				else if (gio.KeyCtrl && _selectedGO && obj != _selectedGO)
				{
					int64 id = obj->GetId(); bool removed = false; // 멀티셀렉트 토글
					for (size_t k = 0; k < _selIds.size(); ++k) if (_selIds[k] == id) { _selIds.erase(_selIds.begin() + k); removed = true; break; }
					if (!removed) _selIds.push_back(id);
					_anchorId = id;
				}
				else { _selIds.clear(); _selectedGO = obj; _anchorId = obj->GetId(); } // 단일 선택
			}
			// 우클릭 컨텍스트 메뉴
			if (!renaming && ImGui::BeginPopupContextItem())
			{
				_selectedGO = obj; // 메뉴 대상 선택
				if (ImGui::MenuItem("Duplicate", "Ctrl+D")) DuplicateSelectedObject();
				if (ImGui::MenuItem("Delete", "Del")) { DeleteSelectedObject(); ImGui::EndPopup(); return; }
				if (ImGui::MenuItem("Group Selected", "Ctrl+G")) GroupSelected();
				if (ImGui::MenuItem("Snap to Grid")) SnapSelectedToGrid();
				if (ImGui::MenuItem("Save as Prefab...")) SaveSelectedAsPrefab();
				if (ImGui::MenuItem("Create Empty Child"))
				{
					Matrix wm0 = obj->GetTransform() ? obj->GetTransform()->GetWorldMatrix() : Matrix{};
					Vec3 wp{ wm0._41, wm0._42, wm0._43 };
					auto child = SpawnEmpty(L"GameObject", wp);
					if (child) if (auto ct = child->GetTransform(), pt = obj->GetTransform(); ct && pt) ct->SetParentKeepWorld(pt);
				}
				if (ImGui::MenuItem("Focus (F)")) { if (auto t = obj->GetTransform()) { Matrix wm0 = t->GetWorldMatrix(); FocusCameraOn(Vec3{ wm0._41, wm0._42, wm0._43 }); } }
				if (ImGui::MenuItem("Drop to Ground"))
				{
					if (auto t = obj->GetTransform())
					{
						Vec3 lp = t->GetLocalPosition(); float gy = 0.f;
						for (auto& kv2 : _gameScene->GetCreatedObjects())
							if (kv2.second) if (auto terr = kv2.second->GetTerrain()) { gy = terr->GetHeight(lp.x, lp.z); break; }
						t->SetLocalPosition(Vec3{ lp.x, gy, lp.z });
					}
				}
				ImGui::EndPopup();
			}
			if (!renaming && ImGui::BeginDragDropSource())
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
		if (!filterLow.empty())
		{
			// 검색 모드: 트리 무시, 이름 매칭 평면 목록 (에디터 내부 제외)
			for (auto& kv : _gameScene->GetCreatedObjects())
			{
				auto& obj = kv.second; if (!obj || obj->IsEditorInternal() || obj == _modelObj) continue;
				std::string nm = WToUtf8(obj->GetObjectName()), low = nm;
				for (char& c : low) c = (char)tolower(c);
				if (low.find(filterLow) == std::string::npos) continue;
				ImGui::PushID((int)(intptr_t)obj->GetId());
				drawTag(typeStyle(obj));
				if (ImGui::Selectable(nm.c_str(), _selectedGO == obj)) _selectedGO = obj;
				ImGui::PopID();
			}
		}
		else
		{
			std::vector<shared_ptr<GameObject>> roots;
			for (auto& kv : _gameScene->GetCreatedObjects())
			{
				auto& obj = kv.second; if (!obj || obj->IsEditorInternal() || obj == _modelObj) continue; // 내부 인프라 + 바닥(=GEO 설정에서 편집) 숨김
				auto t = obj->GetTransform();
				if (!t || !t->GetParent()) roots.push_back(obj);
			}
			for (auto& r : roots) drawNode(r);
		}

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
			if (ImGui::MenuItem("Cylinder")) { BuildPrim(MeshPrim::Cylinder, v, idx); SpawnMeshObject(L"Cylinder", v, idx, spawnAt, MeshPrim::Cylinder); }
			if (ImGui::MenuItem("Cone"))     { BuildPrim(MeshPrim::Cone, v, idx);     SpawnMeshObject(L"Cone", v, idx, spawnAt, MeshPrim::Cone); }
			if (ImGui::MenuItem("Torus"))    { BuildPrim(MeshPrim::Torus, v, idx);    SpawnMeshObject(L"Torus", v, idx, spawnAt, MeshPrim::Torus); }
			if (ImGui::MenuItem("Capsule"))  { BuildPrim(MeshPrim::Capsule, v, idx);  SpawnMeshObject(L"Capsule", v, idx, spawnAt, MeshPrim::Capsule); }
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

	// 단축키: Delete = 삭제, Ctrl+D = 복제, F2 = 리네임 (씬뷰/하이어라키 포커스 시)
	if (_selectedGO && !ImGui::GetIO().WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelectedObject();
		else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) DuplicateSelectedObject();
		else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G)) GroupSelected();
		else if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && !_selectedGO->IsEditorInternal())
		{
			s_renameId = _selectedGO->GetId(); s_renameInit = true;
			std::string cur = WToUtf8(_selectedGO->GetObjectName());
			strncpy_s(s_renameBuf, cur.c_str(), _TRUNCATE);
		}
	}
	// Ctrl+A = 전체 선택 (비-내부, 현재 선택 유무 무관)
	if (_gameScene && !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
	{
		std::vector<int64> ord;
		for (auto& kv : _gameScene->GetCreatedObjects()) if (kv.second && !kv.second->IsEditorInternal()) ord.push_back(kv.first);
		std::sort(ord.begin(), ord.end());
		_selIds.clear();
		if (!ord.empty())
		{
			_selectedGO = _gameScene->GetCreatedObject(ord[0]); _anchorId = ord[0];
			for (size_t k = 1; k < ord.size(); ++k) _selIds.push_back(ord[k]);
		}
	}
	ImGui::End();
}

void D3D12Device::DrawSceneView()
{
	ImGui::Begin("Scene");
	static int s_focusScene = 4; // 시작 시 몇 프레임 Scene 탭을 활성화(기본이 Game 으로 떠 빈 화면으로 보이던 문제)
	if (s_focusScene > 0) { ImGui::SetWindowFocus(); --s_focusScene; }
	// ── Play/Stop ──
	{
		ImVec4 pc = _playing ? ImVec4(0.8f, 0.3f, 0.2f, 1.f) : ImVec4(0.2f, 0.6f, 0.3f, 1.f);
		ImGui::PushStyleColor(ImGuiCol_Button, pc);
		if (ImGui::Button(_playing ? "■ Stop" : "▶ Play")) TogglePlay();
		ImGui::PopStyleColor();
		ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
	}
	// ── 기즈모 툴바 — 활성 모드를 액센트 버튼으로 강조(라디오보다 직관적) ──
	auto toolBtn = [&](const char* label, int op, const char* tip)
	{
		bool active = (_gizmoOp == op);
		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.56f, 0.96f, 1.f));
		bool clicked = ImGui::Button(label);
		if (active) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
		if (clicked) _gizmoOp = op;
		ImGui::SameLine();
	};
	toolBtn("Move", 7, "이동 (W)");
	toolBtn("Rotate", 120, "회전 (E)");
	toolBtn("Scale", 896, "스케일 (R)");
	toolBtn("All", 7 | 120 | 896, "이동+회전+스케일 동시(유니버설)");
	ImGui::TextDisabled("|"); ImGui::SameLine();
	// Local/World 단일 토글 버튼
	if (ImGui::Button(_gizmoLocal ? "Local" : "World")) _gizmoLocal = !_gizmoLocal;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("기즈모 좌표계 (클릭 전환)");
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &_snapOn);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("그리드 스냅 (이동 %.1f / 회전 %.0f° / 스케일 %.1f)", _snapT, _snapR, _snapS);
	ImGui::SameLine();
	if (ImGui::Button("Gizmo")) ImGui::OpenPopup("gizmoSettings");
	if (ImGui::BeginPopup("gizmoSettings"))
	{
		ImGui::TextDisabled("Gizmo / Snap");
		ImGui::SetNextItemWidth(140); ImGui::SliderFloat("Size", &_gizmoSize, 0.05f, 0.4f, "%.2f");
		ImGui::SetNextItemWidth(140); ImGui::DragFloat("Move Snap", &_snapT, 0.05f, 0.05f, 10.f);
		ImGui::SetNextItemWidth(140); ImGui::DragFloat("Rotate Snap", &_snapR, 1.f, 1.f, 90.f, "%.0f°");
		ImGui::SetNextItemWidth(140); ImGui::DragFloat("Scale Snap", &_snapS, 0.05f, 0.05f, 5.f);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	// 그리드/스카이/와이어 빠른 토글 (씬뷰 우상단)
	ImGui::Checkbox("Grid", &_showGrid); ImGui::SameLine();
	ImGui::Checkbox("Sky", &_showSky); ImGui::SameLine();
	ImGui::Checkbox("Wire", &_wireframe); ImGui::SameLine();
	ImGui::Checkbox("Stats", &_showStats); ImGui::SameLine();
	if (ImGui::Button("Frame") && _selectedGO && _selectedGO->GetTransform())
	{
		Matrix wm = _selectedGO->GetTransform()->GetWorldMatrix();
		FocusCameraOn(Vec3{ wm._41, wm._42, wm._43 }); // 선택 오브젝트로 이동 + 시선 정렬
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("선택 오브젝트로 카메라 이동+정렬 (F)");
	ImGui::SameLine();
	if (ImGui::Button("Frame All")) FrameAll();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("전체 오브젝트가 보이도록 프레이밍 (Home)");
	ImGui::SameLine(); ImGui::SetNextItemWidth(90);
	ImGui::SliderFloat("Grid", &_gridCell, 0.25f, 5.f, "%.2f");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	_pendingSceneW = (UINT)max(8.0f, avail.x);
	_pendingSceneH = (UINT)max(8.0f, avail.y);
	ImVec2 imgPos = ImGui::GetCursorScreenPos();
	if (_sceneTexId)
		ImGui::Image((ImTextureID)_sceneTexId, avail);

	// 선택 오브젝트 이름 오버레이 (씬 이미지 좌상단)
	if (_selectedGO && !_selectedGO->IsEditorInternal())
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		std::string nm = WToUtf8(_selectedGO->GetObjectName());
		ImVec2 tp(imgPos.x + 8, imgPos.y + 6);
		ImVec2 ts = ImGui::CalcTextSize(nm.c_str());
		dl->AddRectFilled(ImVec2(tp.x - 4, tp.y - 2), ImVec2(tp.x + ts.x + 4, tp.y + ts.y + 2), IM_COL32(0, 0, 0, 140), 3.f);
		dl->AddText(tp, IM_COL32(255, 230, 120, 255), nm.c_str());
	}
	// 씬 우상단 FPS/ms 오버레이
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		char fb[48]; snprintf(fb, sizeof(fb), "%.0f FPS  %.1f ms", ImGui::GetIO().Framerate, 1000.0f / max(1.0f, ImGui::GetIO().Framerate));
		ImVec2 ts = ImGui::CalcTextSize(fb);
		ImVec2 tp(imgPos.x + avail.x - ts.x - 10, imgPos.y + 6);
		dl->AddRectFilled(ImVec2(tp.x - 4, tp.y - 2), ImVec2(tp.x + ts.x + 4, tp.y + ts.y + 2), IM_COL32(0, 0, 0, 120), 3.f);
		dl->AddText(tp, IM_COL32(140, 230, 140, 255), fb);
	}
	// 씬 통계 오버레이 (Stats 토글) — 프레임타임 그래프 + 카운트 (우상단 FPS 아래)
	if (_showStats)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float pw = 168.f, gph = 38.f;
		ImVec2 o(imgPos.x + avail.x - pw - 10, imgPos.y + 30);
		int objCount = _gameScene ? (int)_gameScene->GetCreatedObjects().size() : 0;
		char l1[64], l2[64];
		snprintf(l1, sizeof(l1), "objs %d   tris %u", objCount, _scene._indexCount / 3);
		snprintf(l2, sizeof(l2), "probes %u   DXR", Ddgi::PROBE_COUNT);
		float panelH = gph + 8 + ImGui::GetTextLineHeight() * 2 + 10;
		dl->AddRectFilled(o, ImVec2(o.x + pw, o.y + panelH), IM_COL32(0, 0, 0, 130), 4.f);
		// 프레임타임 막대 그래프 (_frameTimes, 0~40ms 정규화)
		float gx = o.x + 4, gy = o.y + 4, gw = pw - 8;
		dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gph), IM_COL32(20, 22, 28, 160), 2.f);
		for (int k = 0; k < 120; ++k)
		{
			float ms = _frameTimes[(_frameIdx + k) % 120];
			float h = gph * min(1.0f, ms / 40.0f);
			float x = gx + gw * k / 120.0f;
			ImU32 c = ms > 33.3f ? IM_COL32(230, 90, 80, 220) : ms > 20.f ? IM_COL32(230, 200, 90, 220) : IM_COL32(110, 200, 120, 220);
			dl->AddLine(ImVec2(x, gy + gph), ImVec2(x, gy + gph - h), c);
		}
		dl->AddText(ImVec2(o.x + 6, gy + gph + 4), IM_COL32(200, 210, 220, 255), l1);
		dl->AddText(ImVec2(o.x + 6, gy + gph + 4 + ImGui::GetTextLineHeight()), IM_COL32(160, 180, 200, 255), l2);
	}

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
		// .mat 드롭 → 커서 아래 오브젝트(픽킹)에 공유 머티리얼 할당
		if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MAT_PATH"))
		{
			std::wstring matPath = Utf8ToW(std::string((const char*)pl->Data));
			ImVec2 mp = ImGui::GetMousePos();
			float u = (mp.x - imgPos.x) / avail.x, v = (mp.y - imgPos.y) / avail.y;
			if (u >= 0 && u <= 1 && v >= 0 && v <= 1) PickAt(u, v); // 커서 아래 오브젝트 선택
			if (_selectedGO) if (auto mr = _selectedGO->GetMeshRenderer())
			{
				auto shared = GET_SINGLE(ResourceManager)->Get<Material>(matPath);
				if (!shared) { shared = LoadMaterial(matPath); if (shared) GET_SINGLE(ResourceManager)->Add<Material>(matPath, shared); }
				if (shared) { mr->SetMaterialRef(shared); Log("Assigned material (drop): " + WToUtf8(fs::path(matPath).filename().wstring())); }
			}
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

	// ── ImGuizmo: 선택 GameObject 트랜스폼 조작 (이미지 영역 오버레이) ──
	bool goGizmo = _selectedGO && _selectedGO != _modelObj && _selectedGO->GetTransform() && !_selectedGO->IsEditorInternal() && !terrainEditing;
	if (goGizmo)
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetGizmoSizeClipSpace(_gizmoSize); // V6
		ImGuizmo::SetRect(imgPos.x, imgPos.y, avail.x, avail.y);
		auto t = _selectedGO->GetTransform();
		DirectX::XMFLOAT4X4 wm = t->GetWorldMatrix();
		DirectX::XMFLOAT4X4 oldWM = wm; // 그룹 이동 델타 계산용
		float snapVal = (_gizmoOp == 7) ? _snapT : (_gizmoOp == 120) ? _snapR : _snapS;
		float snap3[3] = { snapVal, snapVal, snapVal };
		if (ImGuizmo::Manipulate((const float*)&_viewM, (const float*)&_projM,
			(ImGuizmo::OPERATION)_gizmoOp, _gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
			(float*)&wm, nullptr, _snapOn ? snap3 : nullptr))
		{
			t->SetWorldMatrix(wm); // 월드→로컬 역산 + 분해 (부모 회전/스케일 안전)
			// 멀티셀렉트 그룹 변환 — 월드 델타 W=inverse(old)·new 를 나머지 선택에 적용(이동/회전/스케일, 피벗=기즈모 중심)
			if (!_selIds.empty())
			{
				using namespace DirectX;
				XMMATRIX oldW = XMLoadFloat4x4(&oldWM), newW = XMLoadFloat4x4(&wm);
				XMMATRIX W = XMMatrixMultiply(XMMatrixInverse(nullptr, oldW), newW);
				for (int64 id : _selIds)
					if (auto o = _gameScene->GetCreatedObject(id))
						if (auto ot = o->GetTransform())
						{
							XMFLOAT4X4 ow = ot->GetWorldMatrix();
							XMFLOAT4X4 nw; XMStoreFloat4x4(&nw, XMMatrixMultiply(XMLoadFloat4x4(&ow), W));
							ot->SetWorldMatrix(nw);
						}
			}
		}
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

	// ── 씬뷰 단축키 (Unity 풍): 호버/포커스 + 텍스트 입력 아님 + RMB 카메라 비행 중 아님 ──
	//   W 이동 / E 회전 / R 스케일 / Q 전체  · F 포커스 · X 로컬↔월드 · H 숨김 · Esc 선택해제
	if ((_sceneHovered || _sceneFocused) && !ImGui::GetIO().WantTextInput && !ImGui::IsMouseDown(1) && !ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) _gizmoOp = 7;          // TRANSLATE
		else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) _gizmoOp = 120;   // ROTATE
		else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) _gizmoOp = 896;   // SCALE
		else if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) _gizmoOp = 7 | 120 | 896; // UNIVERSAL
		if (ImGui::IsKeyPressed(ImGuiKey_X, false)) _gizmoLocal = !_gizmoLocal; // 로컬↔월드
		if (ImGui::IsKeyPressed(ImGuiKey_F, false) && _selectedGO)         // 선택으로 포커스
			if (auto t = _selectedGO->GetTransform()) { Matrix wm = t->GetWorldMatrix(); FocusCameraOn(Vec3{ wm._41, wm._42, wm._43 }); }
		if (ImGui::IsKeyPressed(ImGuiKey_H, false) && _selectedGO && !_selectedGO->IsEditorInternal())
			_selectedGO->SetActive(!_selectedGO->IsActive());            // 숨김 토글
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { _selectedGO = nullptr; _selIds.clear(); _anchorId = -1; } // 선택 해제
		if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) FrameAll();        // 전체 프레이밍
	}
	// 마우스 휠: RMB 비행 중이면 이동 속도 조절(Unity 풍), 평상시엔 시선 방향 도리 줌(앞=확대)
	if (_sceneHovered || _sceneFocused)
	{
		float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.0f)
		{
			if (ImGui::IsMouseDown(1))
				_camera.moveSpeed = max(0.5f, min(50.0f, _camera.moveSpeed * (1.0f + wheel * 0.12f)));
			else
			{
				using namespace DirectX;
				float step = wheel * 0.6f * (ImGui::GetIO().KeyShift ? _camera.fastMul : 1.0f);
				XMStoreFloat3(&_camera.pos, XMVectorAdd(_camera.Eye(), XMVectorScale(_camera.Forward(), step)));
			}
		}
	}

	// ── 카메라 프리뷰 인셋 (선택된 게임 카메라 — Unity 풍, 씬뷰 우하단) ──
	//   RenderGameView 가 선택 카메라를 _gameRT(_gameTexId)에 렌더 → 여기서 작게 오버레이.
	if (_gameTexId && _selectedGO && _selectedGO->IsActive() && !_selectedGO->IsEditorInternal() && _selectedGO->GetCamera())
	{
		float pw = min(320.0f, avail.x * 0.30f);
		float ph = pw * (float)_gameH / (float)max(1u, _gameW); // RT 종횡비 유지
		const float m = 12.0f, titleH = 18.0f;
		ImVec2 imgEnd(imgPos.x + avail.x, imgPos.y + avail.y);
		ImVec2 p0(imgEnd.x - pw - m, imgEnd.y - ph - m);
		ImVec2 p1(p0.x + pw, p0.y + ph);
		ImVec2 t0(p0.x, p0.y - titleH);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(t0, ImVec2(p1.x, p0.y), IM_COL32(25, 25, 30, 230));           // 타이틀바
		dl->AddRectFilled(ImVec2(p0.x, p0.y - 1), p1, IM_COL32(0, 0, 0, 255));           // 배경
		dl->AddImage((ImTextureID)_gameTexId, p0, p1);
		dl->AddRect(t0, p1, IM_COL32(255, 255, 255, 160));                               // 테두리
		std::string label = "Camera Preview  " + WToUtf8(_selectedGO->GetObjectName());
		dl->AddText(ImVec2(t0.x + 5, t0.y + 3), IM_COL32(220, 220, 220, 255), label.c_str());
	}

	ImGui::End();

	DrawGameView(); // "Game" 도킹 창 (게임 카메라 시점)
}

void D3D12Device::DrawLog()
{
	ImGui::Begin("Log");
	if (ImGui::Button("Clear")) _log.clear();
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		std::ofstream lf(_assetRoot + L"\\..\\engine_log.txt");
		if (lf) { for (auto& m : _log) lf << m << '\n'; Log("Log saved: engine_log.txt"); }
	}
	ImGui::SameLine();
	static bool autoScroll = true; ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine(); ImGui::TextDisabled("(%d)", (int)_log.size());
	static char logFilter[64] = "";
	ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##logfilter", "Filter...", logFilter, sizeof(logFilter));
	std::string flt = logFilter; for (char& c : flt) c = (char)tolower(c);
	ImGui::Separator();
	ImGui::BeginChild("logscroll");
	auto contains = [](const std::string& s, const char* k) { return s.find(k) != std::string::npos; };
	for (auto& m : _log)
	{
		if (!flt.empty()) { std::string low = m; for (char& c : low) c = (char)tolower(c); if (low.find(flt) == std::string::npos) continue; }
		// 심각도별 색: 실패/에러=빨강, 성공(saved/loaded/created/converted/OK)=초록, 그 외=기본
		ImVec4 col(0.85f, 0.86f, 0.88f, 1.f);
		if (contains(m, "FAIL") || contains(m, "failed") || contains(m, "error") || contains(m, "ERROR")) col = ImVec4(1.f, 0.45f, 0.40f, 1.f);
		else if (contains(m, "saved") || contains(m, "loaded") || contains(m, "Created") || contains(m, "converted") || contains(m, "generated"))
			col = ImVec4(0.55f, 0.90f, 0.55f, 1.f);
		ImGui::PushStyleColor(ImGuiCol_Text, col);
		ImGui::TextUnformatted(m.c_str());
		ImGui::PopStyleColor();
	}
	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
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
void D3D12Device::PickAt(float u, float v)
{
	using namespace DirectX;
	_selIds.clear(); // 씬 클릭 = 단일 선택(멀티셀렉트 해제)
	float nx = u * 2.0f - 1.0f, ny = (1.0f - v) * 2.0f - 1.0f;
	XMMATRIX invVP = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_viewM) * XMLoadFloat4x4(&_projM));
	XMVECTOR n = XMVector4Transform(XMVectorSet(nx, ny, 0, 1), invVP);
	XMVECTOR f = XMVector4Transform(XMVectorSet(nx, ny, 1, 1), invVP);
	n = XMVectorScale(n, 1.0f / XMVectorGetW(n));
	f = XMVectorScale(f, 1.0f / XMVectorGetW(f));
	XMFLOAT3 ro; XMStoreFloat3(&ro, n);
	XMFLOAT3 rdv; XMStoreFloat3(&rdv, XMVector3Normalize(XMVectorSubtract(f, n)));

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

	// 가장 가까운 히트 선택 (씬그래프 오브젝트 > 바닥)
	if (hitGO && (!hitF || tGO <= tFloor)) { _selectedGO = hitGO; _sel = SelEntity::Model; } // Model=오브젝트 선택 센티넬
	else if (hitF) { _selectedGO = nullptr; _sel = SelEntity::Floor; }
}

