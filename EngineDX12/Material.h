#pragma once
#include "Common.h"
#include <fstream>
#include <sstream>
#include <filesystem>

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
	auto u2w = [](const std::string& s) -> std::wstring {
		if (s == "-" || s.empty()) return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring w(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n); return w;
	};
	// 텍스처 경로를 .mat 폴더 기준으로 절대화 (상대/파일명만 저장된 경우 — 레거시·이식 자산)
	std::filesystem::path matDir = std::filesystem::path(path).parent_path();
	auto resolveTex = [&](const std::wstring& tex) -> std::wstring {
		if (tex.empty()) return tex;
		std::filesystem::path p(tex); std::error_code ec;
		if (p.is_absolute() && std::filesystem::exists(p, ec)) return tex;
		std::filesystem::path cand = matDir / p.filename();    // .mat 폴더의 같은 이름 파일
		if (std::filesystem::exists(cand, ec)) return cand.wstring();
		return tex; // 못 찾으면 원본 유지
	};

	// 바이너리로 통째 읽어 포맷 판별 (신규 텍스트 vs DX11 레거시 바이너리)
	std::ifstream f(path, std::ios::binary); if (!f) return nullptr;
	std::string buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	auto m = make_shared<Material>(); m->_path = path;

	bool isText = buf.rfind("diffuse", 0) == 0 || buf.rfind("metallic", 0) == 0 || buf.rfind("roughness", 0) == 0;
	if (isText)
	{
		std::istringstream f2(buf); std::string line;
		while (std::getline(f2, line))
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
	}
	else
	{
		// DX11 레거시 바이너리: [u32 len + 문자열] × 4(이름/디퓨즈/스펙/노멀) + 색상 float4 ×4(ambient/diffuse/specular/emissive)
		size_t off = 0;
		auto readStr = [&](std::string& out) -> bool {
			if (off + 4 > buf.size()) return false;
			uint32 L; memcpy(&L, buf.data() + off, 4); off += 4;
			if (L > 4096 || off + L > buf.size()) return false;
			out.assign(buf.data() + off, L); off += L; return true;
		};
		auto readF = [&](float& v) { if (off + 4 <= buf.size()) { memcpy(&v, buf.data() + off, 4); off += 4; } };
		std::string name, dtex, stex, ntex;
		if (readStr(name) && readStr(dtex) && readStr(stex) && readStr(ntex))
		{
			float amb[4]{ 1,1,1,1 }, dif[4]{ 1,1,1,1 };
			for (float& a : amb) readF(a);
			for (float& d : dif) readF(d);
			m->_diffuse = { dif[0], dif[1], dif[2] };
			m->_diffuseTex = resolveTex(u2w(dtex));
			m->_specTex    = resolveTex(u2w(stex));
			m->_normalTex  = resolveTex(u2w(ntex));
		}
		else return nullptr; // 알 수 없는 포맷
	}
	// 텍스처 경로 절대화 (텍스트 포맷에서 상대/파일명만 저장된 경우 포함)
	m->_diffuseTex = resolveTex(m->_diffuseTex);
	m->_normalTex  = resolveTex(m->_normalTex);
	m->_specTex    = resolveTex(m->_specTex);
	return m;
}
