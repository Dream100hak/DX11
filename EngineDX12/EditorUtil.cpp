#include "EditorUtil.h"
#include "GeometryHelper.h"
#include "Material.h"
#include "ResourceManager.h"
#include "imgui.h"
#include <cmath>
#include <filesystem>

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

// 유니티/언리얼식 머티리얼 슬롯 — 이름 + 드롭 타겟 + Pick 팝업 (MeshRenderer/ModelAnimator 공용)
void MaterialSlotGUI(const std::wstring& assetRoot, const std::shared_ptr<Material>& cur,
                     const std::function<void(std::shared_ptr<Material>)>& onAssign)
{
	auto wToU = [](const std::wstring& w) { if (w.empty()) return std::string(); int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr); std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr); return s; };
	auto uToW = [](const char* u) { int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0'); if (n > 0) MultiByteToWideChar(CP_UTF8, 0, u, -1, w.data(), n); return w; };
	auto assign = [&](const std::wstring& wp) { if (wp.empty()) return; auto sh = GET_SINGLE(ResourceManager)->Get<Material>(wp); if (!sh) { sh = LoadMaterial(wp); if (sh) GET_SINGLE(ResourceManager)->Add<Material>(wp, sh); } if (sh) onAssign(sh); };

	// 디퓨즈 틴트 색 스웨치 (프리뷰)
	if (cur)
	{
		ImVec4 sw(cur->_diffuse.x, cur->_diffuse.y, cur->_diffuse.z, 1.0f);
		ImGui::ColorButton("##matsw", sw, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(18, 18));
		ImGui::SameLine();
	}
	std::string slot = (cur && !cur->_path.empty()) ? wToU(std::filesystem::path(cur->_path).stem().wstring()) : "(inline material)";
	ImGui::Button(("Mat: " + slot).c_str(), ImVec2(-72, 0)); // 슬롯(드롭 타겟)
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MAT_PATH")) assign(uToW((const char*)pl->Data));
		ImGui::EndDragDropTarget();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(".mat 을 여기로 드래그하거나 Pick 으로 선택");
	ImGui::SameLine();
	if (ImGui::Button("Pick##matslot")) ImGui::OpenPopup("matpick");
	if (ImGui::BeginPopup("matpick"))
	{
		ImGui::TextDisabled("Materials"); ImGui::Separator();
		int shown = 0;
		namespace fs = std::filesystem; std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(assetRoot, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (!it->is_regular_file(ec) || it->path().extension() != L".mat") continue;
			std::string nm = wToU(it->path().filename().wstring());
			bool sel = cur && it->path().wstring() == cur->_path;
			if (ImGui::Selectable(nm.c_str(), sel)) { assign(it->path().wstring()); ImGui::CloseCurrentPopup(); }
			++shown;
		}
		if (shown == 0) ImGui::TextDisabled("(no .mat assets found)");
		ImGui::EndPopup();
	}
}

// 텍스처 슬롯 — 라벨 + 파일명 버튼(드롭 타겟) + Pick(이미지 목록) + Clear
void TextureSlotGUI(const char* label, const std::wstring& assetRoot, const std::wstring& cur,
                    const std::function<void(std::wstring)>& onSet,
                    const std::function<uint64(const std::wstring&)>& getThumb)
{
	auto wToU = [](const std::wstring& w) { if (w.empty()) return std::string(); int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr); std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr); return s; };
	auto uToW = [](const char* u) { int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0'); if (n > 0) MultiByteToWideChar(CP_UTF8, 0, u, -1, w.data(), n); return w; };
	auto isImg = [](const std::wstring& e) { return e == L".png" || e == L".jpg" || e == L".jpeg" || e == L".dds" || e == L".tga" || e == L".bmp"; };

	ImGui::PushID(label);
	ImGui::TextDisabled("%s", label); ImGui::SameLine(72.0f);
	// 썸네일 미리보기 (옵션)
	if (!cur.empty() && getThumb)
	{
		uint64 tid = getThumb(cur);
		if (tid) { ImGui::Image((ImTextureID)tid, ImVec2(20, 20)); ImGui::SameLine(); }
	}
	std::string name = cur.empty() ? "(none)" : wToU(std::filesystem::path(cur).filename().wstring());
	ImGui::Button(name.c_str(), ImVec2(-92, 0));
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEX_PATH")) onSet(uToW((const char*)pl->Data));
		ImGui::EndDragDropTarget();
	}
	if (ImGui::IsItemHovered() && !cur.empty()) ImGui::SetTooltip("%s", wToU(cur).c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton("Pick")) ImGui::OpenPopup("texpick");
	ImGui::SameLine();
	if (ImGui::SmallButton("X")) onSet(std::wstring());
	if (ImGui::BeginPopup("texpick"))
	{
		ImGui::TextDisabled("Images"); ImGui::Separator();
		int shown = 0;
		namespace fs = std::filesystem; std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(assetRoot, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (!it->is_regular_file(ec)) continue;
			std::wstring ext = it->path().extension().wstring();
			for (auto& c : ext) c = (wchar_t)towlower(c);
			if (!isImg(ext)) continue;
			std::string nm = wToU(it->path().filename().wstring());
			if (ImGui::Selectable(nm.c_str())) { onSet(it->path().wstring()); ImGui::CloseCurrentPopup(); }
			if (++shown >= 200) break; // 과다 방지
		}
		if (shown == 0) ImGui::TextDisabled("(no images found)");
		ImGui::EndPopup();
	}
	ImGui::PopID();
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
