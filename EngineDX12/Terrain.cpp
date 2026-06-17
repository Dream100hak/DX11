#include "Terrain.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// windows.h 의 min/max 매크로 충돌 회피용 헬퍼 (std::min/max/clamp 불가)
static inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int   Clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Terrain::Init(int gridN, float cellSize)
{
	_gridN = max(2, gridN);
	_cellSize = cellSize > 0.f ? cellSize : 1.0f;
	_heightmap.assign((size_t)(_gridN + 1) * (_gridN + 1), 0.f);

	auto mr = GetGameObject() ? GetGameObject()->GetMeshRenderer() : nullptr;
	if (!mr) return;
	vector<Vtx> v; vector<uint32> idx;
	BuildVerts(v); BuildIndices(idx);
	mr->SetGeometry(v, idx);
}

// 하이트맵 → 정점. 위치는 월드(그리드 중심 원점), 노멀은 중앙차분, 색은 높이 그라데이션(잔디→바위→눈).
void Terrain::BuildVerts(vector<Vtx>& out) const
{
	const int N = _gridN;
	const float half = HalfSize();
	out.resize((size_t)(N + 1) * (N + 1));

	auto H = [&](int x, int z) -> float
	{
		x = Clampi(x, 0, N); z = Clampi(z, 0, N);
		return _heightmap[(size_t)z * (N + 1) + x];
	};

	for (int z = 0; z <= N; ++z)
		for (int x = 0; x <= N; ++x)
		{
			Vtx& vt = out[(size_t)z * (N + 1) + x];
			float h = _heightmap[(size_t)z * (N + 1) + x];
			vt.pos = XMFLOAT3(x * _cellSize - half, h, z * _cellSize - half);

			// 중앙차분 노멀
			float hl = H(x - 1, z), hr = H(x + 1, z), hd = H(x, z - 1), hu = H(x, z + 1);
			XMVECTOR n = XMVector3Normalize(XMVectorSet(hl - hr, 2.f * _cellSize, hd - hu, 0.f));
			XMStoreFloat3(&vt.nrm, n);
			vt.tan = XMFLOAT3(1, 0, 0);
			vt.uv = XMFLOAT2((float)x / N * 16.f, (float)z / N * 16.f);

			// 페인트 레이어가 있으면 그대로, 없으면 높이/경사 기반 절차적 색(잔디/바위/눈)
			if (!_paint.empty())
				vt.col = _paint[(size_t)z * (N + 1) + x];
			else
			{
				float slope = 1.f - XMVectorGetY(n);             // 0=평지,1=절벽
				XMFLOAT3 grass(0.28f, 0.42f, 0.18f), rock(0.34f, 0.30f, 0.26f), snow(0.92f, 0.94f, 0.97f);
				float snowT = Clampf((h - 6.f) / 6.f, 0.f, 1.f);
				float rockT = Clampf(slope * 2.2f, 0.f, 1.f);
				XMFLOAT3 c;
				c.x = grass.x * (1 - rockT) + rock.x * rockT; c.y = grass.y * (1 - rockT) + rock.y * rockT; c.z = grass.z * (1 - rockT) + rock.z * rockT;
				c.x = c.x * (1 - snowT) + snow.x * snowT; c.y = c.y * (1 - snowT) + snow.y * snowT; c.z = c.z * (1 - snowT) + snow.z * snowT;
				vt.col = c;
			}
		}
}

void Terrain::BuildIndices(vector<uint32>& out) const
{
	const int N = _gridN;
	out.clear(); out.reserve((size_t)N * N * 6);
	for (int z = 0; z < N; ++z)
		for (int x = 0; x < N; ++x)
		{
			uint32 i0 = (uint32)(z * (N + 1) + x);
			uint32 i1 = i0 + 1;
			uint32 i2 = (uint32)((z + 1) * (N + 1) + x);
			uint32 i3 = i2 + 1;
			// 좌수 CW (메인 PSO 컬링 규약과 일치)
			out.push_back(i0); out.push_back(i2); out.push_back(i1);
			out.push_back(i1); out.push_back(i2); out.push_back(i3);
		}
}

void Terrain::UploadVerts()
{
	auto mr = GetGameObject() ? GetGameObject()->GetMeshRenderer() : nullptr;
	if (!mr) return;
	vector<Vtx> v; BuildVerts(v);
	mr->UpdateVertices(v);
}

void Terrain::Sculpt(float wx, float wz, float radius, float strength, TerrainBrush mode, float flattenH)
{
	const int N = _gridN;
	const float half = HalfSize();
	// 월드 → 그리드 좌표
	float gx = (wx + half) / _cellSize;
	float gz = (wz + half) / _cellSize;
	float gr = radius / _cellSize;

	int x0 = max(0, (int)std::floor(gx - gr)), x1 = min(N, (int)std::ceil(gx + gr));
	int z0 = max(0, (int)std::floor(gz - gr)), z1 = min(N, (int)std::ceil(gz + gr));
	if (x0 > x1 || z0 > z1) return;

	// Smooth 는 원본 스냅샷에서 평균
	std::vector<float> snapshot;
	if (mode == TerrainBrush::Smooth) snapshot = _heightmap;

	for (int z = z0; z <= z1; ++z)
		for (int x = x0; x <= x1; ++x)
		{
			float dx = x - gx, dz = z - gz;
			float d = std::sqrt(dx * dx + dz * dz);
			if (d > gr) continue;
			float fall = 0.5f + 0.5f * std::cos(d / gr * 3.14159265f); // 코사인 폴오프
			float& h = _heightmap[(size_t)z * (N + 1) + x];
			switch (mode)
			{
			case TerrainBrush::Raise:  h += strength * fall; break;
			case TerrainBrush::Lower:  h -= strength * fall; break;
			case TerrainBrush::Flatten: h += (flattenH - h) * min(1.f, fall * strength); break;
			case TerrainBrush::Smooth:
			{
				auto S = [&](int xx, int zz) { xx = Clampi(xx, 0, N); zz = Clampi(zz, 0, N); return snapshot[(size_t)zz * (N + 1) + xx]; };
				float avg = (S(x - 1, z) + S(x + 1, z) + S(x, z - 1) + S(x, z + 1) + S(x, z)) * 0.2f;
				h += (avg - h) * min(1.f, fall * strength);
				break;
			}
			}
		}
	UploadVerts();
}

void Terrain::Paint(float wx, float wz, float radius, float strength, const Vec3& color)
{
	const int N = _gridN;
	const float half = HalfSize();
	// 페인트 레이어 지연 초기화 — 현재 절차적 색을 베이스로 복사
	if (_paint.empty())
	{
		vector<Vtx> base; BuildVerts(base); // _paint 비었으므로 절차적 색 생성
		_paint.resize(base.size());
		for (size_t i = 0; i < base.size(); ++i) _paint[i] = base[i].col;
	}

	float gx = (wx + half) / _cellSize, gz = (wz + half) / _cellSize, gr = radius / _cellSize;
	int x0 = max(0, (int)std::floor(gx - gr)), x1 = min(N, (int)std::ceil(gx + gr));
	int z0 = max(0, (int)std::floor(gz - gr)), z1 = min(N, (int)std::ceil(gz + gr));
	if (x0 > x1 || z0 > z1) return;

	for (int z = z0; z <= z1; ++z)
		for (int x = x0; x <= x1; ++x)
		{
			float dx = x - gx, dz = z - gz, d = std::sqrt(dx * dx + dz * dz);
			if (d > gr) continue;
			float fall = (0.5f + 0.5f * std::cos(d / gr * 3.14159265f)) * min(1.f, strength);
			Vec3& c = _paint[(size_t)z * (N + 1) + x];
			c.x += (color.x - c.x) * fall; c.y += (color.y - c.y) * fall; c.z += (color.z - c.z) * fall;
		}
	UploadVerts();
}

float Terrain::GetHeight(float wx, float wz) const
{
	const int N = _gridN;
	const float half = HalfSize();
	float gx = (wx + half) / _cellSize;
	float gz = (wz + half) / _cellSize;
	if (gx < 0 || gz < 0 || gx > N || gz > N) return 0.f;
	int x0 = (int)std::floor(gx), z0 = (int)std::floor(gz);
	int x1 = min(N, x0 + 1), z1 = min(N, z0 + 1);
	float fx = gx - x0, fz = gz - z0;
	float h00 = _heightmap[(size_t)z0 * (N + 1) + x0], h10 = _heightmap[(size_t)z0 * (N + 1) + x1];
	float h01 = _heightmap[(size_t)z1 * (N + 1) + x0], h11 = _heightmap[(size_t)z1 * (N + 1) + x1];
	float a = h00 * (1 - fx) + h10 * fx;
	float b = h01 * (1 - fx) + h11 * fx;
	return a * (1 - fz) + b * fz;
}

// 높이필드 레이마치: 광선을 일정 간격 전진하며 표면 아래로 들어가는 구간을 이분탐색
bool Terrain::Raycast(const Vec3& ro, const Vec3& rd, Vec3& hit) const
{
	const float half = HalfSize();
	float t = 0.f, tMax = 1000.f;
	float step = _cellSize * 0.5f;
	float prevT = 0.f; bool prevBelow = false; bool have = false;

	for (t = 0.f; t < tMax; t += step)
	{
		float px = ro.x + rd.x * t, pz = ro.z + rd.z * t, py = ro.y + rd.y * t;
		if (px < -half || px > half || pz < -half || pz > half) { if (have) break; else { have = true; continue; } }
		have = true;
		float h = GetHeight(px, pz);
		bool below = py <= h;
		if (below && t > 0.f)
		{
			// 이분탐색 (prevT .. t)
			float lo = prevBelow ? t - step : prevT, hi = t;
			for (int i = 0; i < 16; ++i)
			{
				float mid = (lo + hi) * 0.5f;
				float mx = ro.x + rd.x * mid, mz = ro.z + rd.z * mid, my = ro.y + rd.y * mid;
				if (my <= GetHeight(mx, mz)) hi = mid; else lo = mid;
			}
			float ft = (lo + hi) * 0.5f;
			hit = Vec3{ ro.x + rd.x * ft, ro.y + rd.y * ft, ro.z + rd.z * ft };
			return true;
		}
		prevT = t; prevBelow = below;
	}
	return false;
}

void Terrain::CopyFrom(const Terrain& src)
{
	_gridN = src._gridN; _cellSize = src._cellSize;
	_heightmap = src._heightmap; _paint = src._paint;
	auto mr = GetGameObject() ? GetGameObject()->GetMeshRenderer() : nullptr;
	if (mr) { vector<Vtx> v; vector<uint32> idx; BuildVerts(v); BuildIndices(idx); mr->SetGeometry(v, idx); }
}

bool Terrain::SaveHeightmap(const std::wstring& path)
{
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	int32 n = _gridN; float cs = _cellSize;
	f.write((const char*)&n, sizeof(n));
	f.write((const char*)&cs, sizeof(cs));
	f.write((const char*)_heightmap.data(), _heightmap.size() * sizeof(float));
	// 페인트 레이어 (구버전 파일은 이 블록 없음 — 로드 시 eof 로 graceful)
	uint32 hasPaint = _paint.empty() ? 0u : 1u;
	f.write((const char*)&hasPaint, sizeof(hasPaint));
	if (hasPaint) f.write((const char*)_paint.data(), _paint.size() * sizeof(Vec3));
	_hmPath = path;
	return true;
}

bool Terrain::LoadHeightmap(const std::wstring& path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	int32 n = 0; float cs = 1.f;
	f.read((char*)&n, sizeof(n));
	f.read((char*)&cs, sizeof(cs));
	if (n < 2 || n > 4096) return false;
	_gridN = n; _cellSize = cs;
	_heightmap.assign((size_t)(n + 1) * (n + 1), 0.f);
	f.read((char*)_heightmap.data(), _heightmap.size() * sizeof(float));
	// 페인트 레이어 (있으면 로드, 없으면 절차적 색)
	_paint.clear();
	uint32 hasPaint = 0;
	if (f.read((char*)&hasPaint, sizeof(hasPaint)) && hasPaint)
	{
		_paint.assign((size_t)(n + 1) * (n + 1), Vec3{ 0,0,0 });
		f.read((char*)_paint.data(), _paint.size() * sizeof(Vec3));
	}
	_hmPath = path;

	auto mr = GetGameObject() ? GetGameObject()->GetMeshRenderer() : nullptr;
	if (mr) { vector<Vtx> v; vector<uint32> idx; BuildVerts(v); BuildIndices(idx); mr->SetGeometry(v, idx); }
	return true;
}
