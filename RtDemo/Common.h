#pragma once
// RtDemo — DX12 + DXR(레이트레이싱) 렌더러. 기존 DX11 에디터와 별개 프로젝트.
// 목표: DDGI(Dynamic Diffuse Global Illumination) 를 하드웨어 레이트레이싱으로 구현.
// 공용 헤더 (DX11 엔진의 pch 역할 — PCH 미사용, 일반 include).

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;

using uint32 = uint32_t;
using uint64 = uint64_t;

// HRESULT 체크 — 실패 시 즉시 중단 (디버그 레이어 메시지와 함께 추적)
inline void ThrowIfFailed(HRESULT hr, const char* msg = "D3D12 call failed")
{
	if (FAILED(hr))
	{
		char buf[256];
		sprintf_s(buf, "%s (hr=0x%08X)", msg, static_cast<unsigned>(hr));
		throw std::runtime_error(buf);
	}
}
