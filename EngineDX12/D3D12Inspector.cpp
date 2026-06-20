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

// 인스펙터 / 프로젝트 / 폴더 콘텐츠 패널 (D3D12Editor.cpp 에서 분리)
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

	// 선택 엔티티 색상 헤더 (계층 태그 색과 일치 — 무엇을 편집 중인지 즉시 인지)
	{
		struct H { const char* name; ImVec4 col; };
		H h{ "Settings", ImVec4(0.70f,0.70f,0.75f,1) };
		switch (_sel)
		{
		case SelEntity::Camera: h = { "Editor Camera",     ImVec4(0.40f,0.78f,0.92f,1) }; break;
		case SelEntity::Sun:    h = { "Directional Light", ImVec4(0.96f,0.80f,0.32f,1) }; break;
		case SelEntity::DDGI:   h = { "DDGI Volume",        ImVec4(0.66f,0.52f,0.86f,1) }; break;
		case SelEntity::Point:  h = { "Point Light",        ImVec4(0.95f,0.62f,0.32f,1) }; break;
		case SelEntity::Spot:   h = { "Spot Light",         ImVec4(0.52f,0.70f,1.00f,1) }; break;
		case SelEntity::Post:   h = { "Post / Render",      ImVec4(0.42f,0.82f,0.70f,1) }; break;
		case SelEntity::Model:  h = { "Model",              ImVec4(0.58f,0.68f,0.82f,1) }; break;
		case SelEntity::Floor:  h = { "Floor / Ground",     ImVec4(0.74f,0.58f,0.36f,1) }; break;
		}
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		float fh = ImGui::GetTextLineHeight();
		dl->AddRectFilled(p, ImVec2(p.x + 4, p.y + fh), ImGui::GetColorU32(h.col), 2.f); // 좌측 색 바
		ImGui::Indent(10.0f); ImGui::TextColored(h.col, "%s", h.name); ImGui::Unindent(10.0f);
		ImGui::Separator();
	}

	// 하이어라키 선택 대상별 프로퍼티 (고정 엔티티)
	switch (_sel)
	{
	case SelEntity::Camera:
		ImGui::SeparatorText("Editor Camera");
		ImGui::Text("Pos   %.2f  %.2f  %.2f", _camera.pos.x, _camera.pos.y, _camera.pos.z);
		ImGui::Text("Yaw %.2f   Pitch %.2f", _camera.yaw, _camera.pitch);
		ImGui::SliderFloat("FOV", &_camera.fov, 20.0f, 100.0f);              // T1
		ImGui::TextDisabled("Lens:"); ImGui::SameLine();
		if (ImGui::SmallButton("Wide 24mm")) _camera.fov = 73.7f;  ImGui::SameLine();
		if (ImGui::SmallButton("50mm"))      _camera.fov = 39.6f;  ImGui::SameLine();
		if (ImGui::SmallButton("Tele 85mm")) _camera.fov = 23.9f;
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
		{ static float sunK = 6500.f; ImGui::SetNextItemWidth(-60); if (ImGui::SliderFloat("Temp (K)##sun", &sunK, 1500.f, 12000.f, "%.0fK")) _sunColor = KelvinToRGB(sunK); HelpMarker("색온도로 태양색 설정 — 2700K 백열등(주황), 5500K 한낮, 8000K+ 그늘(파랑)."); }
		ImGui::SliderFloat("Env Intensity", &_envIntensity, 0.0f, 3.0f);
		ImGui::Checkbox("Animate Sun", &_lightAnimate);
		if (!_lightAnimate) ImGui::SliderFloat("Sun Angle", &_lightAngle, -3.14159f, 3.14159f);
		ImGui::SliderFloat("Shadow Softness", &_shadowSoft, 0.0f, 0.12f); // T11
		ImGui::Checkbox("Time of Day", &_todOn);                          // V4
		if (_todOn) ImGui::SliderFloat("Hour", &_timeOfDay, 0.0f, 1.0f);
		ImGui::SeparatorText("Sky / IBL");                              // T20
		ImGui::Checkbox("IBL (env cubemap)", &_iblOn);
		HelpMarker("큐브맵을 SH 로 베이크한 이미지 기반 앰비언트 — PBR 머티리얼이 환경색을 받아 제대로 보입니다.\n반사 미스도 환경색으로 폴백. 데저트 큐브맵 기준.");
		if (_iblOn) ImGui::SliderFloat("IBL Intensity", &_iblIntensity, 0.0f, 3.0f);
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
		ImGui::SameLine(); if (ImGui::Button("Overcast")) { _skyZenith = {0.55f,0.57f,0.60f}; _skyHorizon = {0.70f,0.72f,0.75f}; _sunColor = {0.85f,0.86f,0.88f}; _lightIntensity = 0.7f; _shadowStrength = 0.4f; _hemiAmbient = 0.5f; }
		ImGui::SameLine(); if (ImGui::Button("Dawn"))     { _skyZenith = {0.18f,0.20f,0.40f}; _skyHorizon = {0.92f,0.62f,0.45f}; _sunColor = {1.0f,0.72f,0.5f}; _lightIntensity = 0.9f; _lightAngle = 1.25f; _lightAnimate = false; }
		break;

	case SelEntity::DDGI:
		ImGui::SeparatorText("DDGI Volume");
		ImGui::SliderFloat("GI Strength", &_giStrength, 0.0f, 1.5f);
		ImGui::SliderFloat("Ambient", &_ambient, 0.0f, 0.2f);
		ImGui::Checkbox("Show Probes", &_probeViz);                      // T15
		ImGui::Spacing();
		// 스로틀 — 매 프레임 프로브 전부 갱신 대신 1/N 라운드로빈 (GI 비용 절감, 약간의 응답 지연)
		ImGui::Checkbox("Throttle (round-robin)", &_ddgiThrottle);
		HelpMarker("매 프레임 프로브를 1/N 만 갱신해 GI 비용을 줄입니다. N 프레임마다 전체 1회전.\n동적 조명 반응이 약간 느려지지만 EMA 누적으로 잔상은 적습니다.");
		if (_ddgiThrottle)
		{
			const char* divs[] = { "1/2 (2 frames)", "1/4 (4 frames)", "1/5 (5 frames)", "1/10 (10 frames)" };
			const int divVal[] = { 2, 4, 5, 10 };
			int cur = 0; for (int i = 0; i < 4; ++i) if (divVal[i] == _ddgiDiv) cur = i;
			ImGui::SetNextItemWidth(160);
			if (ImGui::Combo("Refresh", &cur, divs, 4)) _ddgiDiv = divVal[cur];
		}
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
		{ static float ptK = 3200.f; ImGui::SetNextItemWidth(-60); if (ImGui::SliderFloat("Temp (K)##pt0", &ptK, 1500.f, 12000.f, "%.0fK")) _pointColor = KelvinToRGB(ptK); }
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
		ImGui::Checkbox("Auto Exposure", &_autoExp); HelpMarker("화면 평균 휘도에 맞춰 노출을 자동 조절. Target=목표 밝기, x값=현재 적용 배율.");
		if (_autoExp) { ImGui::SliderFloat("Target", &_expTarget, 0.1f, 1.5f); ImGui::SameLine(); ImGui::TextDisabled("(x%.2f)", _expScale); }
		ImGui::SeparatorText("Depth of Field / God Rays");
		ImGui::Checkbox("DOF", &_dofOn); HelpMarker("피사계 심도 — Focus Dist 거리에 초점, 그 밖은 블러. Focus Range=선명 유지 범위.");
		ImGui::SliderFloat("Focus Dist", &_dofFocus, 1.0f, 30.0f);
		ImGui::SliderFloat("Focus Range", &_dofRange, 0.5f, 15.0f);
		ImGui::Checkbox("Volumetric Rays", &_volOn); HelpMarker("갓레이(빛 산란) — 그림자 사이로 빛줄기. Ray Strength=세기.");
		ImGui::SliderFloat("Ray Strength", &_volStrength, 0.0f, 2.0f);
		ImGui::SeparatorText("Bloom");
		ImGui::Checkbox("Bloom", &_bloomOn);
		ImGui::SliderFloat("Threshold", &_bloomThreshold, 0.2f, 3.0f);
		ImGui::SliderFloat("Bloom Intensity", &_bloomIntensity, 0.0f, 2.0f);
		ImGui::SeparatorText("Color Grading");
		ImGui::TextDisabled("Presets:"); ImGui::SameLine();
		if (ImGui::SmallButton("Neutral"))   { _contrast = 1.0f; _saturation = 1.0f; _temperature = 0.0f; _vignette = 0.25f; } ImGui::SameLine();
		if (ImGui::SmallButton("Warm"))      { _contrast = 1.08f; _saturation = 1.12f; _temperature = 0.35f; _vignette = 0.3f; } ImGui::SameLine();
		if (ImGui::SmallButton("Cool"))      { _contrast = 1.06f; _saturation = 0.95f; _temperature = -0.35f; _vignette = 0.3f; } ImGui::SameLine();
		if (ImGui::SmallButton("Cinematic")) { _contrast = 1.25f; _saturation = 0.85f; _temperature = -0.12f; _vignette = 0.55f; } ImGui::SameLine();
		if (ImGui::SmallButton("B&W"))       { _contrast = 1.2f; _saturation = 0.0f; _temperature = 0.0f; _vignette = 0.45f; }
		ImGui::SliderFloat("Contrast", &_contrast, 0.5f, 2.0f);
		ImGui::SliderFloat("Saturation", &_saturation, 0.0f, 2.0f);
		ImGui::SliderFloat("Temperature", &_temperature, -1.0f, 1.0f); HelpMarker("색온도 — 음수=차갑게(파랑), 양수=따뜻하게(주황).");
		ImGui::SliderFloat("Vignette", &_vignette, 0.0f, 1.0f);
		ImGui::SeparatorText("Fog / AA / Reflection");
		ImGui::ColorEdit3("Fog Color", &_fogColor.x);
		ImGui::SliderFloat("Fog Density", &_fogDensity, 0.0f, 0.08f);
		ImGui::Checkbox("Height Fog", &_heightFog);
		if (_heightFog) { ImGui::SliderFloat("Fog Height", &_fogHeight, -5.f, 20.f); ImGui::SliderFloat("Fog Falloff", &_fogFalloff, 0.02f, 2.f); }
		ImGui::SeparatorText("Anti-Aliasing");
		ImGui::Checkbox("TAA (temporal)", &_taaOn);
		HelpMarker("시간적 AA — 서브픽셀 지터를 프레임마다 누적해 엣지/셰이더 지터를 정리.\nDDGI/RT/스키닝 노이즈에 특히 효과적. 빠른 모션엔 약간의 잔상(이웃 클램프로 억제).");
		ImGui::SameLine(); ImGui::Checkbox("FXAA", &_fxaaOn);
		if (_taaOn && _fxaaOn) ImGui::TextDisabled("(TAA 활성 시 FXAA 무시)");
		ImGui::Checkbox("RT Reflection", &_reflectOn); HelpMarker("레이트레이싱 반사 — 거울/금속 표면에 씬 반사. Strength=혼합 비율.");
		ImGui::SliderFloat("Reflect Strength", &_reflectStrength, 0.0f, 1.0f);
		ImGui::SeparatorText("Ambient Occlusion (RT)");
		ImGui::Checkbox("RT AO", &_aoOn); HelpMarker("레이트레이싱 앰비언트 오클루전 — 틈/접합부 음영. Radius=샘플 반경.");
		ImGui::SliderFloat("AO Intensity", &_aoIntensity, 0.0f, 2.0f);
		ImGui::SliderFloat("AO Radius", &_aoRadius, 0.1f, 2.0f);
		// 전역 셰이딩/아웃라인 (모든 메시 공통 — 모델 인스펙터에서 이전)
		ImGui::SeparatorText("Shading / Outline");
		ImGui::SliderFloat("Normal Map", &_normalIntensity, 0.0f, 3.0f); HelpMarker("노멀맵 강도(법선 섭동 배율).");
		ImGui::SliderInt("Toon Levels", &_toonLevels, 0, 6); HelpMarker("툰 셰이딩 계단 수(0=끔).");
		ImGui::SliderFloat("Rim Power", &_rimPower, 0.0f, 8.0f); ImGui::ColorEdit3("Rim Color", &_rimColor.x);
		ImGui::ColorEdit3("Outline Color", &_outlineColor.x); ImGui::SliderFloat("Outline Width", &_outlineThick, 0.0f, 0.03f);
		ImGui::SeparatorText("Lens FX");
		ImGui::SliderFloat("Chromatic Aberr.", &_chroma, 0.0f, 0.02f);
		ImGui::SliderFloat("Film Grain", &_grain, 0.0f, 0.2f);
		ImGui::SliderFloat("Sharpen", &_sharpen, 0.0f, 1.5f);
		ImGui::SliderFloat("Lens Distort", &_lensDistort, -0.5f, 0.5f);   // V10
		ImGui::SliderFloat("Posterize", &_posterize, 0.0f, 16.0f);       // V11 (0=off)
		HelpMarker("색 계조 수 제한(만화풍). 0=끔, 낮을수록 단계가 거칠어짐.");
		ImGui::Combo("Filter", &_filterMode, "None\0Sepia\0Grayscale\0Invert\0"); // V12
		ImGui::Checkbox("Anamorphic Bloom", &_anamorphic);               // V19
		HelpMarker("가로로 늘어진 시네마틱 블룸(아나모픽 렌즈 느낌).");
		ImGui::SliderFloat("Render Scale", &_renderScale, 0.5f, 2.0f);
		HelpMarker("내부 렌더 해상도 배율. <1=성능↑(저해상), >1=슈퍼샘플링(고품질·느림).");
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
		HelpMarker("절두체 컬링 — 화면 밖 오브젝트 드로우 생략(성능). 컬링 버그 의심 시 끄고 확인.");
		ImGui::Checkbox("Show Bones", &_showBones); ImGui::SameLine(); ImGui::Checkbox("AABB", &_showAABB);
		ImGui::Checkbox("Light Icons", &_showLightIcons); ImGui::SameLine(); ImGui::Checkbox("Spot Cone", &_showSpotCone);
		ImGui::SeparatorText("Frame Time");
		ImGui::PlotLines("ms", _frameTimes, 120, _frameIdx % 120, nullptr, 0.0f, 40.0f, ImVec2(0, 60));
		ImGui::Text("%.2f ms  /  %.0f FPS", _frameTimes[(_frameIdx + 119) % 120], ImGui::GetIO().Framerate);
		// 다중 데칼 (상향 투영, 최대 8) — 바닥/터레인/프리미티브에 적용
		ImGui::SeparatorText("Decals (multi, max 8)");
		if ((int)_decals.size() < 8 && ImGui::Button("+ Add Decal"))
			_decals.push_back({ SpawnPoint(), 2.f, Vec3{ 0.8f, 0.1f, 0.1f } });
		for (int di = 0; di < (int)_decals.size(); ++di)
		{
			ImGui::PushID(2000 + di);
			ImGui::DragFloat3("Pos", &_decals[di].pos.x, 0.05f);
			ImGui::ColorEdit3("Col", &_decals[di].color.x); ImGui::SameLine();
			ImGui::SetNextItemWidth(80); ImGui::DragFloat("R", &_decals[di].radius, 0.05f, 0.2f, 20.f);
			ImGui::SameLine(); if (ImGui::SmallButton("X")) { _decals.erase(_decals.begin() + di); ImGui::PopID(); break; }
			ImGui::PopID();
		}
		break;

	case SelEntity::Floor:
		ImGui::SeparatorText("Floor / Ground");                        // U9 / U19
		ImGui::Checkbox("Visible", &_showFloor); HelpMarker("바닥 표시(끄면 래스터+RT 그림자/GI 에서 모두 제외). View 메뉴에서도 토글.");
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
	// 현재 폴더에 새 폴더 생성
	if (ImGui::SmallButton("New Folder"))
	{
		std::error_code fec;
		std::wstring base = _curDir.empty() ? _assetRoot : _curDir;
		for (int n = 0; n < 100; ++n)
		{
			std::wstring cand = base + L"\\NewFolder" + (n ? std::to_wstring(n) : L"");
			if (!fs::exists(cand, fec)) { fs::create_directories(cand, fec); Log("Created folder: " + WToUtf8(cand)); break; }
		}
	}
	ImGui::Separator();
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
	if (!go->IsEditorInternal())
	{
		if (ImGui::SmallButton("Duplicate")) DuplicateSelectedObject();
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete")) { DeleteSelectedObject(); ImGui::Separator(); return; }
	}
	ImGui::Separator();

	const bool internal = go->IsEditorInternal();
	// 컴포넌트 타입별 색 (계층 색 언어와 통일)
	auto compColor = [&](ComponentType ct) -> ImVec4
	{
		switch (ct)
		{
		case ComponentType::Camera:   return ImVec4(0.40f,0.78f,0.92f,1);
		case ComponentType::Light:    return ImVec4(0.96f,0.80f,0.32f,1);
		case ComponentType::Terrain:  return ImVec4(0.74f,0.58f,0.36f,1);
		case ComponentType::Collider: return ImVec4(0.50f,0.82f,0.52f,1);
		case ComponentType::Renderer:
			if (go->GetRenderer()) switch (go->GetRenderer()->GetRenderType())
			{
			case RendererType::Animator:  return ImVec4(0.52f,0.82f,0.66f,1);
			case RendererType::Particle:  return ImVec4(0.72f,0.52f,0.88f,1);
			case RendererType::Foliage:   return ImVec4(0.48f,0.78f,0.42f,1);
			case RendererType::Billboard: return ImVec4(0.88f,0.66f,0.52f,1);
			default: return ImVec4(0.58f,0.68f,0.82f,1);
			}
			return ImVec4(0.58f,0.68f,0.82f,1);
		default: return ImVec4(0.62f,0.62f,0.68f,1); // Transform 등 중립
		}
	};
	for (uint8 i = 0; i < static_cast<uint8>(ComponentType::End); ++i)
	{
		ComponentType ct = static_cast<ComponentType>(i);
		auto c = go->GetFixedComponent(ct);
		if (!c) continue;

		const char* compName = (ct == ComponentType::Renderer && go->GetRenderer())
			? ImGuiManager::EnumToString(go->GetRenderer()->GetRenderType())
			: ImGuiManager::EnumToString(ct);

		// 접을 수 있는 컴포넌트 헤더 + 좌측 색 바 + 우측 X 제거 버튼
		ImGui::PushID(i);
		bool open = ImGui::CollapsingHeader(compName, ImGuiTreeNodeFlags_DefaultOpen);
		{ ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
		  ImGui::GetWindowDrawList()->AddRectFilled(mn, ImVec2(mn.x + 3, mx.y), ImGui::GetColorU32(compColor(ct)), 1.f); }
		bool canRemove = ct != ComponentType::Transform && !internal && !(go == _modelObj && ct == ComponentType::Renderer);
		if (canRemove)
		{
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 6.0f);
			if (ImGui::SmallButton("X")) { go->RemoveComponent(ct); ImGui::PopID(); continue; }
		}
		if (open) c->OnInspectorGUI();
		ImGui::PopID();
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
			if (GetOpenFileNameW(&ofn) && terr->LoadHeightmap(file))
			{
				Log("Heightmap loaded: " + WToUtf8(file));
				// 이 터레인에 식생이 있으면 새 지형 높이에 맞춰 자동 재배치 (자식 링크로 조회 — 리네임 무관)
				if (auto tt = go->GetTransform())
					for (auto& ct : tt->GetChildren())
						if (ct) if (auto cgo = ct->GetGameObject())
							if (auto fol = std::dynamic_pointer_cast<Foliage>(cgo->GetRenderer()))
							{ fol->Regenerate(terr.get()); Log("Foliage re-scattered on loaded terrain"); break; }
			}
		}

		// ── Foliage (잔디/나무) ──
		ImGui::SeparatorText("Foliage");
		ImGui::DragInt("Grass", &_folGrass, 50, 0, 50000);
		ImGui::DragInt("Trees", &_folTree, 1, 0, 2000);
		ImGui::DragFloat("Grass Size", &_folSize, 0.02f, 0.05f, 3.f);
		ImGui::DragInt("Seed", &_folSeed, 1, 0, 1000000);
		if (ImGui::Button("Generate Foliage", ImVec2(-1, 0))) GenerateFoliage(go);
	}

	{
		shared_ptr<MonoBehaviour> removeScript;
		int si = 0;
		for (auto& s : go->GetMonoBehaviours())
		{
			if (!s) continue;
			const char* tn = s->TypeName(); ImGui::SeparatorText(tn ? tn : "Script"); // 널 가드
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
			if (!go->GetCamera() && ImGui::MenuItem("Camera"))
			{ go->AddComponent(make_shared<Camera>()); Log("Added Camera to " + WToUtf8(go->GetObjectName())); }
			if (!go->GetRenderer() && !go->GetTerrain() && ImGui::MenuItem("Terrain"))
			{
				auto mr = make_shared<MeshRenderer>(); mr->Bind(this); go->AddComponent(mr);
				auto tr = make_shared<Terrain>(); tr->Bind(this); go->AddComponent(tr); tr->Init(128, 1.0f);
				Log("Added Terrain to " + WToUtf8(go->GetObjectName()));
			}
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
			// .mat 드래그 → Scene 뷰 오브젝트에 드롭하면 머티리얼 할당
			if (!isDir && kind == AssetKind::Material && p.extension() == L".mat" && ImGui::BeginDragDropSource())
			{
				std::string up = WToUtf8(full);
				ImGui::SetDragDropPayload("MAT_PATH", up.c_str(), up.size() + 1);
				ImGui::TextUnformatted(WToUtf8(p.filename().wstring()).c_str());
				ImGui::EndDragDropSource();
			}
			// 이미지 드래그 → 인스펙터 텍스처 슬롯(디퓨즈/노멀/스펙)에 드롭
			if (!isDir && kind == AssetKind::Image && ImGui::BeginDragDropSource())
			{
				std::string up = WToUtf8(full);
				ImGui::SetDragDropPayload("TEX_PATH", up.c_str(), up.size() + 1);
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
