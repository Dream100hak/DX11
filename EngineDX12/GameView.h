#pragma once
#include "Common.h"
#include "PostFX.h"

class D3D12Device;

// ───────────────────────────────────────────────────────────
// GameView — 배치된 게임 카메라(비-에디터 Camera GameObject) 시점의 오프스크린 렌더 + 포스트 + "Game" 도킹창.
// D3D12Device 가 값 멤버(_gameView)로 소유하고, 디바이스 내부(_cmdList/_gameScene/PSO/설정 등)는
// _dev 백포인터(friend)로 접근한다. RT/velocity/postfx/창 상태는 이 클래스가 소유.
// (프레임 CB _gameCB 와 에디터 베이스 CB _cbCache 는 디바이스 잔류 — 프레임 리소스/렌더 상태라.)
// ───────────────────────────────────────────────────────────
class GameView
{
public:
	void Init(D3D12Device* dev, DXGI_FORMAT sceneFmt);
	void CreateRT(UINT w, UINT h);   // 오프스크린 RT/깊이/velocity (재)생성
	void Render();                   // 게임 카메라 → RT (Scene 패스 직후 호출)
	void DrawWindow();               // "Game" 도킹 탭

	UINT   Width()  const { return _w; }
	UINT   Height() const { return _h; }
	uint64 TexId()  const { return _texId; }

private:
	D3D12Device* _dev = nullptr;
	DXGI_FORMAT  _sceneFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;

	ComPtr<ID3D12Resource>       _rt, _depth;
	ComPtr<ID3D12DescriptorHeap> _rtvHeap, _dsvHeap;
	D3D12_RESOURCE_STATES        _rtState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES        _depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	// 속도 G버퍼 (모션블러 — 게임 카메라 기준)
	ComPtr<ID3D12Resource>       _velRT;
	ComPtr<ID3D12DescriptorHeap> _velRtvHeap;
	D3D12_RESOURCE_STATES        _velRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	DirectX::XMFLOAT4X4          _prevVP{};
	bool                         _hasPrevVP = false;

	UINT                         _w = 640, _h = 360, _pendingW = 0, _pendingH = 0;
	uint64                       _texId = 0;
	PostFX                       _postfx;
	bool                         _windowOpen = true;
};
