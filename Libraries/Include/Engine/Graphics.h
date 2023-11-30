#pragma once
#include "Viewport.h"

class Texture;
class ShadowMap;

class Graphics
{
	DECLARE_SINGLE(Graphics);

public:
	void Init(HWND hwnd);

	void PreRenderBegin();
	void RenderBegin();
	void RenderEnd();

	ComPtr<ID3D11Device> GetDevice() { return _device; }
	ComPtr<ID3D11DeviceContext> GetDeviceContext() { return _deviceContext; }

	shared_ptr<ShadowMap> GetShadowMap() { return _smap; }

	ComPtr <ID3D11DepthStencilState> GetDSStateStandard() { return _dsStateStandard; }
	ComPtr <ID3D11DepthStencilState> GetDSStateOutline() { return _dsStateOutline; }

private:
	void CreateDeviceAndSwapChain();
	void CreateRenderTargetView();
	void CreateDepthStencilView();

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

	ComPtr<ID3D11DepthStencilState> _dsStateStandard;
	ComPtr<ID3D11DepthStencilState> _dsStateOutline;

	shared_ptr<ShadowMap> _smap = nullptr; 

	Viewport _vp;

};
