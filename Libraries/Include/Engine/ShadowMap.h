#pragma once
#include "Texture.h"

class ShadowMap : public Texture
{
public:
	ShadowMap(uint32 width , uint32 height );
	~ShadowMap();

	void BindDsvAndSetNullRenderTarget();
	void DrawSceneToShadowMap();

	void BuildShadowTransform();

private:
	uint32 _width;
	uint32 _height;

	ComPtr<ID3D11DepthStencilView> _depthMapDSV;

};

