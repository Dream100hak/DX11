#pragma once
#include "Renderer.h"

class MeshThumbnail : public Texture
{
public:
	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

	// drawGrid: 모델 프리뷰 바닥 그리드 표시 여부 (인스펙터 프리뷰만 true, 폴더컨텐츠 썸네일은 false)
	void Draw(vector<shared_ptr<Renderer>> renderers, Matrix V , Matrix P, shared_ptr<Light> light, vector<shared_ptr<class InstancingBuffer>> buffers, bool drawGrid = true);

	// 오브젝트 월드 AABB 기준 자동 핏 V/P (유니티식 3/4 시점, 중앙 정렬)
	// — 고정 카메라는 모델 크기/위치마다 빗나가고, 씬 종횡비 P 는 정사각 RT 에서 찌그러짐
	static void ComputeFitViewProj(shared_ptr<class GameObject> obj, float aspect, Matrix& outV, Matrix& outP);

private:
	void CreateColorTexture();
	void CreateDepthStencilTexture();

	// 모델 프리뷰 바닥 그리드 — 씬 그리드(SceneGrid)와 같은 셰이더/룩, 원점 고정.
	// 모든 썸네일이 공유 (드로우가 순차 실행이라 안전), 모델/애니 렌더러가 있을 때만 깐다.
	void DrawGrid(Matrix V, Matrix P, shared_ptr<Light> light);

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11RenderTargetView> _colorMapRTV;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
	Viewport _vp;

	static shared_ptr<class GameObject> _gridObj;
	static shared_ptr<class SceneGrid>  _grid;
};
