#pragma once
#include "Component.h"
#include "Common.h"

class D3D12Device;

// DX11 Engine/Terrain 이식 (1차: Sculpt) — CPU 하이트맵 그리드.
//   같은 GameObject 의 MeshRenderer 에 정점을 채워 메인 디퍼드 경로로 렌더(추가 PSO 불필요).
//   GameObject 트랜스폼은 항등 유지(정점=월드). 브러시는 CPU 하이트맵 수정 → 노멀 재계산 → in-place 재업로드.
//   Paint(블렌드맵)/Foliage 는 추후 증분.
enum class TerrainBrush : int { Raise, Lower, Smooth, Flatten, Paint };

class Terrain : public Component
{
public:
	Terrain() : Component(ComponentType::Terrain) {}
	void Bind(D3D12Device* dev) { _dev = dev; }

	// gridN×gridN 셀(정점 (gridN+1)²), 셀 크기 cellSize(m). 평지로 초기화 + MeshRenderer 지오메트리 설정.
	void Init(int gridN, float cellSize);

	// 월드 (wx,wz) 중심 반경 radius 브러시. strength: 초당 변화량×dt 적용해 호출측에서 스케일.
	void Sculpt(float wx, float wz, float radius, float strength, TerrainBrush mode, float flattenH);
	// 정점 색 페인트 (셰이더 추가 없이 정점색 경로 활용). color 로 blend.
	void Paint(float wx, float wz, float radius, float strength, const Vec3& color);

	float GetHeight(float wx, float wz) const;             // bilinear 샘플 (그리드 밖이면 0)
	bool  Raycast(const Vec3& ro, const Vec3& rd, Vec3& hit) const; // 높이필드 march + 이분탐색

	bool SaveHeightmap(const std::wstring& path);          // .r32 (float32 raw) + 경로 기록
	bool LoadHeightmap(const std::wstring& path);
	void CopyFrom(const Terrain& src);                     // 하이트맵/페인트 복사 + 메시 재생성(Duplicate용)

	int   GridN() const { return _gridN; }
	float CellSize() const { return _cellSize; }
	float WorldSize() const { return _gridN * _cellSize; }
	float HalfSize() const { return _gridN * _cellSize * 0.5f; }
	const std::wstring& HeightmapPath() const { return _hmPath; }
	const std::vector<float>& Heightmap() const { return _heightmap; } // GPU 테셀레이션 변위용

private:
	void BuildVerts(vector<Vtx>& out) const;     // 하이트맵 → 정점(위치/노멀/색/uv)
	void BuildIndices(vector<uint32>& out) const;
	void UploadVerts();                          // 정점 재생성 후 MeshRenderer 갱신
	int  Idx(int x, int z) const { return z * (_gridN + 1) + x; }

	D3D12Device*       _dev = nullptr;
	int                _gridN = 128;
	float              _cellSize = 1.0f;
	std::vector<float> _heightmap;   // (gridN+1)²
	std::vector<Vec3>  _paint;       // 페인트 레이어(비어있으면 절차적 색 사용). (gridN+1)²
	std::wstring       _hmPath;
};
