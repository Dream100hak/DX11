#pragma once
#include "Common.h"
#include <fstream>
#include <sstream>

// DX11 Engine/Material 이식 — PBR 파라미터 + 텍스처 경로 + .mat 자산화(공유).
class Material
{
public:
	Vec3   _diffuse{ 1.f, 1.f, 1.f }; // 틴트
	float  _metallic = 0.f;
	float  _roughness = 0.5f;
	float  _emissive = 0.f;

	wstring _diffuseTex;
	wstring _normalTex;
	wstring _specTex;

	wstring _path; // .mat 자산 경로 (비어있으면 인라인). 공유 캐시 키.
};

// .mat 텍스트 저장/로드
inline bool SaveMaterial(const Material& m, const std::wstring& path)
{
	std::ofstream f(path); if (!f) return false;
	auto w2u = [](const std::wstring& w) -> std::string {
		if (w.empty()) return "-";
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string s(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr); return s;
	};
	f << "diffuse " << m._diffuse.x << ' ' << m._diffuse.y << ' ' << m._diffuse.z << '\n';
	f << "metallic " << m._metallic << '\n' << "roughness " << m._roughness << '\n' << "emissive " << m._emissive << '\n';
	f << "diffuseTex " << w2u(m._diffuseTex) << '\n' << "normalTex " << w2u(m._normalTex) << '\n' << "specTex " << w2u(m._specTex) << '\n';
	return true;
}

inline shared_ptr<Material> LoadMaterial(const std::wstring& path)
{
	std::ifstream f(path); if (!f) return nullptr;
	auto m = make_shared<Material>(); m->_path = path;
	auto u2w = [](const std::string& s) -> std::wstring {
		if (s == "-" || s.empty()) return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring w(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n); return w;
	};
	std::string line;
	while (std::getline(f, line))
	{
		std::istringstream s(line); std::string t; s >> t;
		if (t == "diffuse") s >> m->_diffuse.x >> m->_diffuse.y >> m->_diffuse.z;
		else if (t == "metallic") s >> m->_metallic;
		else if (t == "roughness") s >> m->_roughness;
		else if (t == "emissive") s >> m->_emissive;
		else if (t == "diffuseTex") { std::string p; std::getline(s >> std::ws, p); m->_diffuseTex = u2w(p); }
		else if (t == "normalTex")  { std::string p; std::getline(s >> std::ws, p); m->_normalTex = u2w(p); }
		else if (t == "specTex")    { std::string p; std::getline(s >> std::ws, p); m->_specTex = u2w(p); }
	}
	return m;
}
