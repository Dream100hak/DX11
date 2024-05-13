#pragma once
#include "Viewport.h"
#include "JobQueue.h"

class Texture;

class Graphics
{
	DECLARE_SINGLE(Graphics);

public:
	void Init(HWND hwnd);

	void PreRenderBegin();
	void RenderBegin();
	void PostRenderBegin();
	void RenderEnd();

	ComPtr<ID3D11Device> GetDevice() { return _device; }
	ComPtr<ID3D11DeviceContext> GetDeviceContext() { return _deviceContext; }
	ComPtr<ID3D11DepthStencilView> GetDsv() { return _depthStencilView; }

	//-- 그림자 , 썸네일 , 프리뷰 등 필요한 일감들은 앞으로 JobQueue에 넣어 관리합니다. -- // 
	shared_ptr<JobQueue>& GetPreRenderJobQueue() { return _preRenderJobQueue;  }
	shared_ptr<JobQueue>& GetRenderJobQueue() { return _renderJobQueue;  }
	shared_ptr<JobQueue>& GetPostRenderJobQueue() { return _postRenderJobQueue;  }

	//----------------------------------------------------------------------------------

	ComPtr <ID3D11DepthStencilState> GetDSStateStandard() { return _dsStateStandard; }
	ComPtr <ID3D11DepthStencilState> GetDSStateOutline() { return _dsStateOutline; }

	ComPtr<ID3D11RasterizerState> GetWireframeRS() { return _wireframeRS ;}

private:
	void CreateDeviceAndSwapChain();
	void CreateRenderTargetView();
	void CreateDepthStencilView();
	void CreateRasterizer();

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

	Viewport _vp;

	ComPtr<ID3D11DepthStencilState> _dsStateStandard;
	ComPtr<ID3D11DepthStencilState> _dsStateOutline;

	shared_ptr<JobQueue> _preRenderJobQueue = nullptr;
	shared_ptr<JobQueue> _renderJobQueue = nullptr;
	shared_ptr<JobQueue> _postRenderJobQueue = nullptr;

	//RS 모음
	ComPtr<ID3D11RasterizerState> _wireframeRS;
};
