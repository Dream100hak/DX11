#include "EditorUtil.h"
#include "GeometryHelper.h"

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
