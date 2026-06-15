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

struct ClipFrameT { DirectX::XMFLOAT3 s; DirectX::XMFLOAT4 r; DirectX::XMFLOAT3 t; };
struct AnimClip
{
	float frameRate = 30.f;
	uint32 frameCount = 0;
	std::unordered_map<std::string, std::vector<ClipFrameT>> bones; // boneName → 프레임별 S/R/T
};

inline bool LoadMeshSkinned(const std::wstring& path, std::vector<LoadedBone>& outBones,
                            std::vector<SkinVtx>& outV, std::vector<uint32>& outI)
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
		r.SkipStr();          // name
		r.Read<int32_t>();    // boneIndex
		r.SkipStr();          // material
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
		for (uint32 k = 0; k < icount; ++k) outI.push_back(base + r.Read<uint32>());
		r.Skip(24);           // MeshAabb
	}
	return !outV.empty();
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
