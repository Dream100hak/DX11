#pragma once
#include "Common.h"
#include <fstream>
#include <cstring>
#include <unordered_map>

// ───────────────────────────────────────────────────────────
// DX11 엔진 .mesh 바이너리 로더 (위치+노멀만 추출 — RT/래스터용 정적 메시).
// 포맷 (FileUtils 직렬화):
//   bones : uint32 count; per { int32 index, str name, int32 parent, Matrix(64) }
//   meshes: uint32 count; per { str name, int32 boneIndex, str material,
//           uint32 vcount + vcount×ModelVertex(76B), uint32 icount + icount×uint32, MeshAabb(24B) }
//   ModelVertex = pos(12) uv(8) normal(12) tangent(12) blendIdx(16) blendW(16) = 76B
//   str = uint32 길이 + 원시 바이트(널 없음)
// ───────────────────────────────────────────────────────────

struct MeshPN { DirectX::XMFLOAT3 pos; DirectX::XMFLOAT3 nrm; };

class MeshBlob
{
	const uint8_t* _p;
	const uint8_t* _end;
public:
	MeshBlob(const uint8_t* d, size_t n) : _p(d), _end(d + n) {}
	bool Ok() const { return _p <= _end; }
	const uint8_t* Ptr() const { return _p; }
	template<class T> T Read() { T v{}; memcpy(&v, _p, sizeof(T)); _p += sizeof(T); return v; }
	void Skip(size_t n) { _p += n; }
	uint32 ReadStrLen() { return Read<uint32>(); }
	void SkipStr() { uint32 n = Read<uint32>(); _p += n; }
};

// 모든 서브메시를 합쳐 위치/노멀 + 인덱스(서브메시별 오프셋 적용) 반환
inline bool LoadMeshPN(const std::wstring& path, std::vector<MeshPN>& outV, std::vector<uint32>& outI)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return false;
	std::streamsize sz = f.tellg();
	if (sz <= 0) return false;
	f.seekg(0);
	std::vector<uint8_t> buf((size_t)sz);
	f.read(reinterpret_cast<char*>(buf.data()), sz);

	MeshBlob r(buf.data(), buf.size());

	uint32 boneCount = r.Read<uint32>();
	for (uint32 i = 0; i < boneCount; ++i)
	{
		r.Read<int32_t>();  // index
		r.SkipStr();        // name
		r.Read<int32_t>();  // parent
		r.Skip(64);         // Matrix
	}

	uint32 meshCount = r.Read<uint32>();
	for (uint32 m = 0; m < meshCount; ++m)
	{
		r.SkipStr();                       // name
		r.Read<int32_t>();                 // boneIndex
		r.SkipStr();                       // material

		uint32 vcount = r.Read<uint32>();
		uint32 base = (uint32)outV.size();
		for (uint32 v = 0; v < vcount; ++v)
		{
			DirectX::XMFLOAT3 pos = r.Read<DirectX::XMFLOAT3>();
			r.Skip(8);                     // uv
			DirectX::XMFLOAT3 nrm = r.Read<DirectX::XMFLOAT3>();
			r.Skip(12 + 16 + 16);          // tangent + blendIndices + blendWeights
			outV.push_back({ pos, nrm });
		}

		uint32 icount = r.Read<uint32>();
		for (uint32 k = 0; k < icount; ++k)
			outI.push_back(base + r.Read<uint32>());

		r.Skip(24);                        // MeshAabb
	}
	return !outV.empty();
}

// ───────────────────────────────────────────────────────────
// 스키닝용 확장 로더 — 본 계층 + 정점 블렌드(인덱스/가중치) + .clip 애니메이션
// ───────────────────────────────────────────────────────────
struct LoadedBone { int32_t parent; std::string name; DirectX::XMFLOAT4X4 bind; };
struct SkinVtx { DirectX::XMFLOAT3 pos; DirectX::XMFLOAT3 nrm; DirectX::XMFLOAT3 tan; DirectX::XMFLOAT2 uv; uint32 idx[4]; float wgt[4]; };

// 서브메시(머티리얼 단위 인덱스 구간) — 다중 머티리얼 드로우용
struct SubMesh { std::string materialName; uint32 indexStart; uint32 indexCount; };

// 경로/이름에서 스템(마지막 \ 또는 / 뒤) 추출 — .mmat 경로 ↔ .mesh 머티리얼명 매칭용
inline std::string PathStem(const std::string& s)
{
	size_t p = s.find_last_of("\\/");
	return (p == std::string::npos) ? s : s.substr(p + 1);
}

struct ClipFrameT { DirectX::XMFLOAT3 s; DirectX::XMFLOAT4 r; DirectX::XMFLOAT3 t; };
struct AnimClip
{
	float frameRate = 30.f;
	uint32 frameCount = 0;
	std::unordered_map<std::string, std::vector<ClipFrameT>> bones; // boneName → 프레임별 S/R/T
};

inline bool LoadMeshSkinned(const std::wstring& path, std::vector<LoadedBone>& outBones,
                            std::vector<SkinVtx>& outV, std::vector<uint32>& outI,
                            std::vector<SubMesh>* outSub = nullptr)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return false;
	std::streamsize sz = f.tellg(); if (sz <= 0) return false;
	f.seekg(0);
	std::vector<uint8_t> buf((size_t)sz);
	f.read(reinterpret_cast<char*>(buf.data()), sz);
	MeshBlob r(buf.data(), buf.size());

	uint32 boneCount = r.Read<uint32>();
	outBones.resize(boneCount);
	for (uint32 i = 0; i < boneCount; ++i)
	{
		int32_t index = r.Read<int32_t>();
		uint32 nlen = r.ReadStrLen();
		std::string name((const char*)r.Ptr(), nlen); r.Skip(nlen);
		int32_t parent = r.Read<int32_t>();
		DirectX::XMFLOAT4X4 bind = r.Read<DirectX::XMFLOAT4X4>();
		if (index < 0 || index >= (int32_t)boneCount) index = (int32_t)i;
		outBones[index] = { parent, name, bind };
	}

	uint32 meshCount = r.Read<uint32>();
	for (uint32 m = 0; m < meshCount; ++m)
	{
		r.SkipStr();                       // name
		r.Read<int32_t>();                 // boneIndex
		uint32 mlen = r.ReadStrLen();      // material 이름(경로일 수 있음)
		std::string matName((const char*)r.Ptr(), mlen); r.Skip(mlen);
		uint32 vcount = r.Read<uint32>();
		uint32 base = (uint32)outV.size();
		for (uint32 v = 0; v < vcount; ++v)
		{
			SkinVtx sv{};
			sv.pos = r.Read<DirectX::XMFLOAT3>();
			sv.uv = r.Read<DirectX::XMFLOAT2>();        // uv
			sv.nrm = r.Read<DirectX::XMFLOAT3>();
			sv.tan = r.Read<DirectX::XMFLOAT3>();       // tangent
			DirectX::XMFLOAT4 bi = r.Read<DirectX::XMFLOAT4>(); // blendIndices(float4)
			DirectX::XMFLOAT4 bw = r.Read<DirectX::XMFLOAT4>(); // blendWeights(float4)
			sv.idx[0] = (uint32)bi.x; sv.idx[1] = (uint32)bi.y; sv.idx[2] = (uint32)bi.z; sv.idx[3] = (uint32)bi.w;
			sv.wgt[0] = bw.x; sv.wgt[1] = bw.y; sv.wgt[2] = bw.z; sv.wgt[3] = bw.w;
			outV.push_back(sv);
		}
		uint32 icount = r.Read<uint32>();
		uint32 idxStart = (uint32)outI.size();
		for (uint32 k = 0; k < icount; ++k) outI.push_back(base + r.Read<uint32>());
		r.Skip(24);           // MeshAabb
		if (outSub) outSub->push_back({ PathStem(matName), idxStart, icount });
	}
	return !outV.empty();
}

// ───────────────────────────────────────────────────────────
// 탄젠트 생성 — .mesh 탄젠트가 0(구 변환 경로)이면 UV+위치로 재계산.
// 바인드 포즈 기준 1회 생성 → 스키닝이 매 프레임 변형(skin 행렬 적용).
// ───────────────────────────────────────────────────────────
inline void GenerateTangents(std::vector<SkinVtx>& v, const std::vector<uint32>& idx)
{
	using namespace DirectX;
	// 이미 유효한 탄젠트가 있으면(0 아님) 건너뜀
	double sumLen = 0.0;
	for (auto& s : v) sumLen += fabs(s.tan.x) + fabs(s.tan.y) + fabs(s.tan.z);
	if (sumLen > 1e-3) return;

	std::vector<XMVECTOR> acc(v.size(), XMVectorZero());
	for (size_t t = 0; t + 2 < idx.size(); t += 3)
	{
		uint32 i0 = idx[t], i1 = idx[t + 1], i2 = idx[t + 2];
		XMVECTOR p0 = XMLoadFloat3(&v[i0].pos), p1 = XMLoadFloat3(&v[i1].pos), p2 = XMLoadFloat3(&v[i2].pos);
		XMFLOAT2 &u0 = v[i0].uv, &u1 = v[i1].uv, &u2 = v[i2].uv;
		XMVECTOR e1 = XMVectorSubtract(p1, p0), e2 = XMVectorSubtract(p2, p0);
		float du1 = u1.x - u0.x, dv1 = u1.y - u0.y, du2 = u2.x - u0.x, dv2 = u2.y - u0.y;
		float det = du1 * dv2 - du2 * dv1;
		if (fabsf(det) < 1e-8f) continue;
		float r = 1.0f / det;
		// T = (e1*dv2 - e2*dv1) * r
		XMVECTOR tan = XMVectorScale(XMVectorSubtract(XMVectorScale(e1, dv2), XMVectorScale(e2, dv1)), r);
		acc[i0] = XMVectorAdd(acc[i0], tan); acc[i1] = XMVectorAdd(acc[i1], tan); acc[i2] = XMVectorAdd(acc[i2], tan);
	}
	for (size_t i = 0; i < v.size(); ++i)
	{
		XMVECTOR n = XMLoadFloat3(&v[i].nrm);
		XMVECTOR t = acc[i];
		// Gram-Schmidt 직교화: t -= n*(n·t)
		t = XMVectorSubtract(t, XMVectorScale(n, XMVectorGetX(XMVector3Dot(n, t))));
		if (XMVectorGetX(XMVector3LengthSq(t)) < 1e-10f) t = XMVector3Cross(n, XMVectorSet(0, 0, 1, 0));
		XMStoreFloat3(&v[i].tan, XMVector3Normalize(t));
	}
}

inline bool LoadClip(const std::wstring& path, AnimClip& out)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return false;
	std::streamsize sz = f.tellg(); if (sz <= 0) return false;
	f.seekg(0);
	std::vector<uint8_t> buf((size_t)sz);
	f.read(reinterpret_cast<char*>(buf.data()), sz);
	MeshBlob r(buf.data(), buf.size());

	r.SkipStr();                       // clipName
	r.Read<float>();                   // duration
	out.frameRate = r.Read<float>();
	out.frameCount = r.Read<uint32>();
	uint32 kfCount = r.Read<uint32>();
	for (uint32 i = 0; i < kfCount; ++i)
	{
		uint32 nlen = r.ReadStrLen();
		std::string boneName((const char*)r.Ptr(), nlen); r.Skip(nlen);
		uint32 size = r.Read<uint32>();
		std::vector<ClipFrameT>& frames = out.bones[boneName];
		frames.resize(size);
		for (uint32 fr = 0; fr < size; ++fr)
		{
			r.Read<float>();                       // time
			frames[fr].s = r.Read<DirectX::XMFLOAT3>();
			frames[fr].r = r.Read<DirectX::XMFLOAT4>();
			frames[fr].t = r.Read<DirectX::XMFLOAT3>();
		}
	}
	return out.frameCount > 0;
}

// ───────────────────────────────────────────────────────────
// .mmat + .mat 파서 — 머티리얼별 디퓨즈/노멀/스펙 텍스처 절대경로 맵 반환.
//   .mmat: int32 count + count×string(머티리얼 .mat 경로)
//   .mat : string shader / string diffuse / string specular / string normal / ...
//   텍스처 파일명은 모델 폴더 기준 상대 → modelDir 와 결합한 절대경로 반환.
// 키 = 머티리얼 스템(.mesh 서브메시 materialName 과 매칭).
// ───────────────────────────────────────────────────────────
struct MatTex { std::wstring diffuse, normal, spec; };

inline std::wstring Utf8ToW(const std::string& s)
{
	if (s.empty()) return L"";
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
	return w;
}

inline std::unordered_map<std::string, MatTex> LoadMaterials(const std::wstring& mmatPath, const std::wstring& modelDir)
{
	std::unordered_map<std::string, MatTex> out;
	std::ifstream f(mmatPath, std::ios::binary | std::ios::ate);
	if (!f) return out;
	std::streamsize sz = f.tellg(); if (sz <= 0) return out;
	f.seekg(0);
	std::vector<uint8_t> buf((size_t)sz);
	f.read(reinterpret_cast<char*>(buf.data()), sz);
	MeshBlob r(buf.data(), buf.size());

	int32_t count = r.Read<int32_t>();
	for (int32_t i = 0; i < count; ++i)
	{
		uint32 len = r.ReadStrLen();
		std::string matRef((const char*)r.Ptr(), len); r.Skip(len);
		std::string stem = PathStem(matRef);

		// <modelDir>\<stem>.mat 열기
		std::wstring matPath = modelDir + Utf8ToW(stem) + L".mat";
		std::ifstream mf(matPath, std::ios::binary | std::ios::ate);
		if (!mf) continue;
		std::streamsize msz = mf.tellg(); if (msz <= 0) continue;
		mf.seekg(0);
		std::vector<uint8_t> mbuf((size_t)msz);
		mf.read(reinterpret_cast<char*>(mbuf.data()), msz);
		MeshBlob mr(mbuf.data(), mbuf.size());

		auto readStr = [&mr]() -> std::string { uint32 n = mr.ReadStrLen(); std::string s((const char*)mr.Ptr(), n); mr.Skip(n); return s; };
		readStr();                       // shader
		std::string diffuse  = readStr();
		std::string specular = readStr();
		std::string normal   = readStr();

		MatTex mt;
		if (!diffuse.length())  mt.diffuse = L"";  else mt.diffuse = modelDir + Utf8ToW(diffuse);
		if (!normal.length())   mt.normal  = L"";  else mt.normal  = modelDir + Utf8ToW(normal);
		if (!specular.length()) mt.spec    = L"";  else mt.spec    = modelDir + Utf8ToW(specular);
		out[stem] = mt;
	}
	return out;
}
