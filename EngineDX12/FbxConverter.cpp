#include "FbxConverter.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cfloat>
#include <functional>

#include <ufbx/ufbx.h>

using namespace DirectX;
namespace fs = std::filesystem;

// ── 직렬화 포맷 (MeshLoader.h 와 바이트 호환) ──
namespace
{
	// VertexType: pos(12) uv(8) normal(12) tangent(12) blendIdx(16) blendW(16) = 76B
	struct ConvVertex
	{
		XMFLOAT3 position{ 0,0,0 };
		XMFLOAT2 uv{ 0,0 };
		XMFLOAT3 normal{ 0,0,0 };
		XMFLOAT3 tangent{ 0,0,0 };
		XMFLOAT4 blendIndices{ 0,0,0,0 };
		XMFLOAT4 blendWeights{ 0,0,0,0 };
	};
	static_assert(sizeof(ConvVertex) == 76, "VertexType must be 76 bytes (포맷 호환)");

	struct MeshAabb { XMFLOAT3 mn{ 0,0,0 }; XMFLOAT3 mx{ 0,0,0 }; };
	static_assert(sizeof(MeshAabb) == 24, "MeshAabb must be 24 bytes");

	// asKeyframeData: time(4) scale(12) rotation(16) translation(12) = 44B
	struct KeyFrame { float time = 0; XMFLOAT3 scale{ 1,1,1 }; XMFLOAT4 rot{ 0,0,0,1 }; XMFLOAT3 trans{ 0,0,0 }; };
	static_assert(sizeof(KeyFrame) == 44, "KeyFrame must be 44 bytes (포맷 호환)");

	struct ConvMesh
	{
		std::string name;
		int32_t boneIndex = -1;
		std::string materialName;
		std::vector<ConvVertex> vertices;
		std::vector<uint32_t> indices;
		MeshAabb aabb;
	};
	struct ConvBone { int32_t index; int32_t parent; std::string name; XMFLOAT4X4 transform; };
	struct ConvMaterial
	{
		std::string name;
		XMFLOAT4 ambient{ 1,1,1,1 }, diffuse{ 1,1,1,1 }, specular{ 0,0,0,1 }, emissive{ 0,0,0,1 };
		float roughness = 0.5f, metallic = 0.f;
		ufbx_texture* diffuseTex = nullptr; ufbx_texture* specularTex = nullptr; ufbx_texture* normalTex = nullptr;
	};
	struct ConvBoneFrames { std::string boneName; std::vector<KeyFrame> frames; };

	struct BinWriter
	{
		std::ofstream f;
		bool Open(const std::wstring& p) { f.open(p, std::ios::binary | std::ios::trunc); return (bool)f; }
		template<class T> void W(const T& v) { f.write(reinterpret_cast<const char*>(&v), sizeof(T)); }
		void WStr(const std::string& s) { uint32_t n = (uint32_t)s.size(); W(n); if (n) f.write(s.data(), n); }
		void WBytes(const void* d, size_t n) { if (n) f.write(reinterpret_cast<const char*>(d), n); }
	};

	XMFLOAT4X4 ToMatrix(const ufbx_matrix& m)
	{
		// ufbx 3x4(열벡터) → 엔진 행벡터 규약 (v*M)
		return XMFLOAT4X4(
			(float)m.cols[0].x, (float)m.cols[0].y, (float)m.cols[0].z, 0.f,
			(float)m.cols[1].x, (float)m.cols[1].y, (float)m.cols[1].z, 0.f,
			(float)m.cols[2].x, (float)m.cols[2].y, (float)m.cols[2].z, 0.f,
			(float)m.cols[3].x, (float)m.cols[3].y, (float)m.cols[3].z, 1.f);
	}
	XMFLOAT3 ToVec3(const ufbx_vec3& v) { return XMFLOAT3((float)v.x, (float)v.y, (float)v.z); }
	XMFLOAT4 ToColor(const ufbx_material_map& map, const XMFLOAT4& fb)
	{
		if (!map.has_value) return fb;
		return XMFLOAT4((float)map.value_vec4.x, (float)map.value_vec4.y, (float)map.value_vec4.z, 1.f);
	}

	std::string WToUtf8(const std::wstring& w)
	{
		if (w.empty()) return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string s(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
		return s;
	}
	std::wstring Utf8ToW(const std::string& s)
	{
		if (s.empty()) return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring w(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
		return w;
	}
}

// ── 본 트리 + 메시 ──
namespace
{
	struct Reader
	{
		ufbx_scene* scene = nullptr;
		std::vector<ConvBone> bones;
		std::vector<ConvMesh> meshes;
		std::unordered_map<const ufbx_node*, int32_t> nodeToBone;

		void ReadModel(ufbx_node* node, int32_t index, int32_t parent)
		{
			ConvBone bone; bone.index = index; bone.parent = parent; bone.name = node->name.data ? node->name.data : "";
			XMFLOAT4X4 localM = ToMatrix(node->node_to_parent);
			XMMATRIX local = XMLoadFloat4x4(&localM);
			XMMATRIX matParent = XMMatrixIdentity();
			if (parent >= 0) matParent = XMLoadFloat4x4(&bones[parent].transform);
			XMStoreFloat4x4(&bone.transform, XMMatrixMultiply(local, matParent));
			bones.push_back(bone);
			nodeToBone[node] = index;

			ReadMesh(node, index);

			for (size_t i = 0; i < node->children.count; ++i)
				ReadModel(node->children.data[i], (int32_t)bones.size(), index);
		}

		void ReadMesh(ufbx_node* node, int32_t boneIndex)
		{
			if (!node->mesh) return;
			ufbx_mesh* sm = node->mesh;
			ConvMesh mesh; mesh.name = node->name.data ? node->name.data : ""; mesh.boneIndex = boneIndex;
			if (node->materials.count > 0) mesh.materialName = node->materials.data[0]->name.data;
			else if (sm->materials.count > 0) mesh.materialName = sm->materials.data[0]->name.data;

			ufbx_skin_deformer* skin = sm->skin_deformers.count > 0 ? sm->skin_deformers.data[0] : nullptr;
			bool hasTangent = sm->vertex_tangent.exists;

			XMFLOAT3 mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			std::vector<uint32_t> tri(sm->max_face_triangles * 3);

			for (size_t fi = 0; fi < sm->faces.count; ++fi)
			{
				ufbx_face face = sm->faces.data[fi];
				uint32_t numTris = ufbx_triangulate_face(tri.data(), tri.size(), sm, face);
				for (uint32_t i = 0; i < numTris * 3; ++i)
				{
					uint32_t ix = tri[i];
					ConvVertex v{};
					v.position = ToVec3(ufbx_get_vertex_vec3(&sm->vertex_position, ix));
					if (sm->vertex_uv.exists) { ufbx_vec2 uv = ufbx_get_vertex_vec2(&sm->vertex_uv, ix); v.uv = XMFLOAT2((float)uv.x, 1.f - (float)uv.y); }
					if (sm->vertex_normal.exists) v.normal = ToVec3(ufbx_get_vertex_vec3(&sm->vertex_normal, ix));
					if (hasTangent) v.tangent = ToVec3(ufbx_get_vertex_vec3(&sm->vertex_tangent, ix));

					if (skin)
					{
						uint32_t vid = sm->vertex_indices.data[ix];
						const ufbx_skin_vertex& sv = skin->vertices.data[vid];
						// (인덱스, 가중치) 상위 4개 + 정규화
						float wgt[4] = { 0,0,0,0 }; int idx[4] = { 0,0,0,0 };
						for (uint32_t w = 0; w < sv.num_weights; ++w)
						{
							const ufbx_skin_weight& sw = skin->weights.data[sv.weight_begin + w];
							ufbx_node* bn = skin->clusters.data[sw.cluster_index]->bone_node;
							if (!bn) continue;
							auto it = nodeToBone.find(bn);
							if (it == nodeToBone.end() || it->second < 0) continue;
							float wv = (float)sw.weight;
							// 가장 작은 슬롯과 교체
							int mnSlot = 0; for (int k = 1; k < 4; ++k) if (wgt[k] < wgt[mnSlot]) mnSlot = k;
							if (wv > wgt[mnSlot]) { wgt[mnSlot] = wv; idx[mnSlot] = it->second; }
						}
						float sum = wgt[0] + wgt[1] + wgt[2] + wgt[3];
						if (sum > 1e-6f) { for (int k = 0; k < 4; ++k) wgt[k] /= sum; }
						v.blendIndices = XMFLOAT4((float)idx[0], (float)idx[1], (float)idx[2], (float)idx[3]);
						v.blendWeights = XMFLOAT4(wgt[0], wgt[1], wgt[2], wgt[3]);
					}

					mn.x = min(mn.x, v.position.x); mn.y = min(mn.y, v.position.y); mn.z = min(mn.z, v.position.z);
					mx.x = max(mx.x, v.position.x); mx.y = max(mx.y, v.position.y); mx.z = max(mx.z, v.position.z);

					mesh.indices.push_back((uint32_t)mesh.vertices.size());
					mesh.vertices.push_back(v);
				}
			}
			mesh.aabb.mn = mn; mesh.aabb.mx = mx;

			if (!hasTangent && sm->vertex_uv.exists) GenerateTangents(mesh);
			meshes.push_back(std::move(mesh));
		}

		void GenerateTangents(ConvMesh& mesh)
		{
			std::vector<XMVECTOR> acc(mesh.vertices.size(), XMVectorZero());
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
			{
				uint32_t i0 = mesh.indices[i], i1 = mesh.indices[i + 1], i2 = mesh.indices[i + 2];
				ConvVertex& v0 = mesh.vertices[i0]; ConvVertex& v1 = mesh.vertices[i1]; ConvVertex& v2 = mesh.vertices[i2];
				XMVECTOR p0 = XMLoadFloat3(&v0.position), p1 = XMLoadFloat3(&v1.position), p2 = XMLoadFloat3(&v2.position);
				XMVECTOR e1 = XMVectorSubtract(p1, p0), e2 = XMVectorSubtract(p2, p0);
				float du1 = v1.uv.x - v0.uv.x, dv1 = v1.uv.y - v0.uv.y, du2 = v2.uv.x - v0.uv.x, dv2 = v2.uv.y - v0.uv.y;
				float det = du1 * dv2 - du2 * dv1; if (fabsf(det) < 1e-8f) continue;
				float r = 1.f / det;
				XMVECTOR tan = XMVectorScale(XMVectorSubtract(XMVectorScale(e1, dv2), XMVectorScale(e2, dv1)), r);
				acc[i0] = XMVectorAdd(acc[i0], tan); acc[i1] = XMVectorAdd(acc[i1], tan); acc[i2] = XMVectorAdd(acc[i2], tan);
			}
			for (size_t v = 0; v < mesh.vertices.size(); ++v)
			{
				XMVECTOR n = XMLoadFloat3(&mesh.vertices[v].normal), t = acc[v];
				t = XMVectorSubtract(t, XMVectorScale(n, XMVectorGetX(XMVector3Dot(n, t))));
				if (XMVectorGetX(XMVector3LengthSq(t)) < 1e-10f) t = XMVector3Cross(n, XMVectorSet(0, 0, 1, 0));
				XMStoreFloat3(&mesh.vertices[v].tangent, XMVector3Normalize(t));
			}
		}
	};

	std::string WriteTexture(const std::wstring& outDir, ufbx_texture* tex)
	{
		if (!tex) return "";
		std::string src = tex->relative_filename.length > 0 ? std::string(tex->relative_filename.data)
			: (tex->filename.length > 0 ? std::string(tex->filename.data) : "");
		std::string fileName = fs::path(src).filename().string();
		if (fileName.empty()) return "";
		std::wstring dst = outDir + Utf8ToW(fileName);

		if (tex->content.size > 0)
		{
			std::ofstream of(dst, std::ios::binary | std::ios::trunc);
			if (of) of.write((const char*)tex->content.data, (std::streamsize)tex->content.size);
		}
		else if (tex->filename.length > 0)
		{
			CopyFileW(Utf8ToW(std::string(tex->filename.data)).c_str(), dst.c_str(), FALSE);
		}
		return fileName;
	}
}

FbxConvertResult ConvertFbxToMesh(const std::wstring& fbxPath, const std::wstring& outDir, const std::wstring& stem)
{
	FbxConvertResult res;

	if (!fs::exists(fbxPath)) { res.error = "FBX not found: " + WToUtf8(fbxPath); return res; }
	std::error_code ec; fs::create_directories(outDir, ec);

	ufbx_load_opts opts{};
	opts.target_axes = ufbx_axes_left_handed_y_up;
	opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	opts.target_unit_meters = 1.0f;
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
	opts.generate_missing_normals = true;

	ufbx_error err;
	ufbx_scene* scene = ufbx_load_file(WToUtf8(fbxPath).c_str(), &opts, &err);
	if (!scene) { res.error = err.description.data ? std::string("ufbx: ") + err.description.data : "ufbx load failed"; return res; }

	Reader rd; rd.scene = scene;
	rd.ReadModel(scene->root_node, -1, -1);

	// ── .mesh ──
	std::wstring meshPath = outDir + stem + L".mesh";
	{
		BinWriter w;
		if (!w.Open(meshPath)) { res.error = "cannot write .mesh"; ufbx_free_scene(scene); return res; }
		w.W<uint32_t>((uint32_t)rd.bones.size());
		for (auto& b : rd.bones) { w.W<int32_t>(b.index); w.WStr(b.name); w.W<int32_t>(b.parent); w.W<XMFLOAT4X4>(b.transform); }
		w.W<uint32_t>((uint32_t)rd.meshes.size());
		for (auto& m : rd.meshes)
		{
			w.WStr(m.name); w.W<int32_t>(m.boneIndex); w.WStr(m.materialName);
			w.W<uint32_t>((uint32_t)m.vertices.size());
			w.WBytes(m.vertices.data(), sizeof(ConvVertex) * m.vertices.size());
			w.W<uint32_t>((uint32_t)m.indices.size());
			w.WBytes(m.indices.data(), sizeof(uint32_t) * m.indices.size());
			w.W<MeshAabb>(m.aabb);
		}
	}

	// ── 머티리얼 (.mat) + .mmat ──
	std::vector<std::string> matFileNames;
	for (size_t i = 0; i < scene->materials.count; ++i)
	{
		ufbx_material* sm = scene->materials.data[i];
		ConvMaterial mat; mat.name = sm->name.data ? sm->name.data : ("mat" + std::to_string(i));
		mat.ambient = ToColor(sm->fbx.ambient_color, XMFLOAT4(1, 1, 1, 1));
		mat.diffuse = ToColor(sm->fbx.diffuse_color, ToColor(sm->pbr.base_color, XMFLOAT4(1, 1, 1, 1)));
		mat.specular = ToColor(sm->fbx.specular_color, XMFLOAT4(0, 0, 0, 1));
		if (sm->fbx.specular_exponent.has_value) mat.specular.w = (float)sm->fbx.specular_exponent.value_real;
		mat.emissive = ToColor(sm->fbx.emission_color, XMFLOAT4(0, 0, 0, 1));
		if (sm->pbr.roughness.has_value) mat.roughness = (float)sm->pbr.roughness.value_real;
		if (sm->pbr.metalness.has_value) mat.metallic = (float)sm->pbr.metalness.value_real;
		mat.diffuseTex = sm->pbr.base_color.texture ? sm->pbr.base_color.texture : sm->fbx.diffuse_color.texture;
		mat.specularTex = sm->fbx.specular_color.texture;
		mat.normalTex = sm->fbx.normal_map.texture ? sm->fbx.normal_map.texture : sm->pbr.normal_map.texture;

		// 머티리얼명을 파일안전 토큰으로 (경로구분/공백 → _)
		std::string safe = mat.name;
		for (char& c : safe) if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
		std::wstring matPath = outDir + Utf8ToW(safe) + L".mat";

		BinWriter mw;
		if (mw.Open(matPath))
		{
			mw.WStr("01. Standard.fx");
			mw.WStr(WriteTexture(outDir, mat.diffuseTex));
			mw.WStr(WriteTexture(outDir, mat.specularTex));
			mw.WStr(WriteTexture(outDir, mat.normalTex));
			mw.W<XMFLOAT4>(mat.ambient); mw.W<XMFLOAT4>(mat.diffuse); mw.W<XMFLOAT4>(mat.specular); mw.W<XMFLOAT4>(mat.emissive);
			mw.W<float>(mat.roughness); mw.W<float>(mat.metallic);
		}
		matFileNames.push_back(safe);
	}
	if (!matFileNames.empty())
	{
		BinWriter mm;
		if (mm.Open(outDir + stem + L".mmat"))
		{
			mm.W<uint32_t>((uint32_t)matFileNames.size());
			std::string folder = WToUtf8(outDir);
			for (auto& nm : matFileNames) mm.WStr(folder + nm); // .mat 경로(폴더+스템); 로더는 PathStem 으로 매칭
		}
	}

	// ── 애니메이션 (.clip) — anim_stack[0] ──
	std::wstring clipPath;
	if (scene->anim_stacks.count > 0)
	{
		ufbx_anim_stack* stack = scene->anim_stacks.data[0];
		float fps = (float)scene->settings.frames_per_second; if (fps <= 0.f) fps = 30.f;
		double tb = stack->time_begin, te = stack->time_end;
		uint32_t frameCount = (uint32_t)(::round((te - tb) * fps)) + 1;
		const double invFps = 1.0 / (double)fps;

		std::vector<ConvBoneFrames> tracks;
		// 본 트리 순회 순서와 동일하게 모든 노드 로컬 트랜스폼 베이크
		std::vector<ufbx_node*> orderStack{ scene->root_node };
		std::vector<ufbx_node*> ordered;
		// 전위 순회(재귀 대신 명시 스택 — 자식 순서 보존 위해 역순 push)
		std::function<void(ufbx_node*)> walk = [&](ufbx_node* n) { ordered.push_back(n); for (size_t i = 0; i < n->children.count; ++i) walk(n->children.data[i]); };
		walk(scene->root_node);

		for (ufbx_node* n : ordered)
		{
			ConvBoneFrames bf; bf.boneName = n->name.data ? n->name.data : "";
			bf.frames.resize(frameCount);
			for (uint32_t f = 0; f < frameCount; ++f)
			{
				double t = tb + (double)f * invFps;
				ufbx_transform tr = ufbx_evaluate_transform(stack->anim, n, t);
				KeyFrame& kf = bf.frames[f];
				kf.time = (float)f;
				kf.scale = ToVec3(tr.scale);
				kf.rot = XMFLOAT4((float)tr.rotation.x, (float)tr.rotation.y, (float)tr.rotation.z, (float)tr.rotation.w);
				kf.trans = ToVec3(tr.translation);
			}
			tracks.push_back(std::move(bf));
		}

		clipPath = outDir + stem + L".clip";
		BinWriter cw;
		if (cw.Open(clipPath))
		{
			cw.WStr(stack->name.data ? std::string(stack->name.data) : "Take");
			cw.W<float>((float)(frameCount - 1)); // duration (틱)
			cw.W<float>(fps);
			cw.W<uint32_t>(frameCount);
			cw.W<uint32_t>((uint32_t)tracks.size());
			for (auto& tr : tracks)
			{
				cw.WStr(tr.boneName);
				cw.W<uint32_t>((uint32_t)tr.frames.size());
				cw.WBytes(tr.frames.data(), sizeof(KeyFrame) * tr.frames.size());
			}
			res.frameCount = (int)frameCount;
		}
	}

	res.ok = true;
	res.boneCount = (int)rd.bones.size();
	res.meshCount = (int)rd.meshes.size();
	res.materialCount = (int)matFileNames.size();
	res.animCount = (int)scene->anim_stacks.count;
	res.meshPath = meshPath;
	res.clipPath = clipPath;
	ufbx_free_scene(scene);
	return res;
}
