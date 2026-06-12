#pragma once
#include "Renderer.h"

class MeshThumbnail : public Texture
{
public:
	MeshThumbnail(uint32 width, uint32 height);
	~MeshThumbnail();

	void Draw(vector<shared_ptr<Renderer>> renderers, Matrix V , Matrix P, shared_ptr<Light> light, vector<shared_ptr<class InstancingBuffer>> buffers);

	// 오브젝트 월드 AABB 기준 자동 핏 V/P (유니티식 3/4 시점, 중앙 정렬)
	// — 고정 카메라는 모델 크기/위치마다 빗나가고, 씬 종횡비 P 는 정사각 RT 에서 찌그러짐
	static void ComputeFitViewProj(shared_ptr<class GameObject> obj, float aspect, Matrix& outV, Matrix& outP);

private:
	void CreateColorTexture();
	void CreateDepthStencilTexture();

	uint32 _width;
	uint32 _height;
	ComPtr<ID3D11RenderTargetView> _colorMapRTV;
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;
	Viewport _vp;
};
