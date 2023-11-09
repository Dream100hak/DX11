#pragma once
#include "Viewport.h"

class Graphics
{
	DECLARE_SINGLE(Graphics);

public:
	void Init(HWND hwnd);

	void RenderBegin();
	void RenderEnd();

	ComPtr<ID3D11Device> GetDevice() { return _device; }
	ComPtr<ID3D11DeviceContext> GetDeviceContext() { return _deviceContext; }

private:
	void CreateDeviceAndSwapChain();
	void CreateRenderTargetView();
	void CreateDepthStencilView();

	void BindStandardRender();
	void BindShadowRender(); 


public:
	void SetViewport(float width, float height, float x = 0, float y = 0, float minDepth = 0, float maxDepth = 1);
	Viewport& GetViewport() { return _vp; }

	bool IsMouseInViewport(int32 x , int32 y)
	{
		return (x >= _vp.GetPosX() && x <= _vp.GetPosX() + _vp.GetWidth() &&
			y >= _vp.GetPosY() && y <= _vp.GetPosY() + _vp.GetHeight());
	}

private:
	HWND _hwnd = {};

	// Device & SwapChain
	ComPtr<ID3D11Device> _device = nullptr;
	ComPtr<ID3D11DeviceContext> _deviceContext = nullptr;
	ComPtr<IDXGISwapChain> _swapChain = nullptr;

	// RTV
	ComPtr<ID3D11RenderTargetView> _renderTargetView;

	// DSV
	ComPtr<ID3D11Texture2D> _depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> _depthStencilView;

	//±Ì¿Ã∏  ¿˙¿ÂøÎ
	ComPtr<ID3D11DepthStencilView> _depthMapDSV;

	Viewport _vp;
};
