#include "EditorUtil.h"
#include "GeometryHelper.h"
#include "imgui.h"
#include <cmath>

// 색온도(Kelvin) → 정규화 RGB (Tanner Helland 근사). 1000~40000K 클램프.
Vec3 KelvinToRGB(float kelvin)
{
	float t = (kelvin < 1000.f ? 1000.f : (kelvin > 40000.f ? 40000.f : kelvin)) / 100.f;
	float r, g, b;
	if (t <= 66.f) r = 255.f;
	else          r = 329.698727446f * powf(t - 60.f, -0.1332047592f);
	if (t <= 66.f) g = 99.4708025861f * logf(t) - 161.1195681661f;
	else           g = 288.1221695283f * powf(t - 60.f, -0.0755148492f);
	if (t >= 66.f) b = 255.f;
	else if (t <= 19.f) b = 0.f;
	else           b = 138.5177312231f * logf(t - 10.f) - 305.0447927307f;
	auto cl = [](float x) { return x < 0.f ? 0.f : (x > 255.f ? 255.f : x); };
	return Vec3{ cl(r) / 255.f, cl(g) / 255.f, cl(b) / 255.f };
}

// "(?)" 호버 툴팁 — 직전 위젯과 같은 줄에 표시
void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// wstring → UTF-8 (ImGui 텍스트용)
std::string WToUtf8(const std::wstring& w)
{
	if (w.empty()) return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

// 프리미티브 종류 → 지오메트리 생성 (재생성/스폰 공용)
void BuildPrim(MeshPrim prim, vector<Vtx>& v, vector<uint32>& idx)
{
	switch (prim) {
	case MeshPrim::Sphere:   GeometryHelper::CreateSphere(v, idx, 0.5f); break;
	case MeshPrim::Plane:    GeometryHelper::CreatePlane(v, idx, 2.0f);  break;
	case MeshPrim::Cylinder: GeometryHelper::CreateCylinder(v, idx, 0.5f, 1.0f); break;
	case MeshPrim::Cone:     GeometryHelper::CreateCone(v, idx, 0.5f, 1.0f); break;
	case MeshPrim::Torus:    GeometryHelper::CreateTorus(v, idx, 0.35f, 0.15f); break;
	case MeshPrim::Capsule:  GeometryHelper::CreateCapsule(v, idx, 0.35f, 0.6f); break;
	case MeshPrim::Cube:
	default:                 GeometryHelper::CreateCube(v, idx, 1.0f);   break;
	}
}
