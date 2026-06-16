#pragma once
// RtDemo — DX12 + DXR(레이트레이싱) 렌더러. 기존 DX11 에디터와 별개 프로젝트.
// 목표: DDGI(Dynamic Diffuse Global Illumination) 를 하드웨어 레이트레이싱으로 구현.
// 공용 헤더 (DX11 엔진의 pch 역할 — PCH 미사용, 일반 include).

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

using int8  = int8_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// DX11 Engine 관례 — 스마트포인터/컨테이너 별칭 (씬그래프 포팅용)
using std::shared_ptr;
using std::weak_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;
using std::static_pointer_cast;
using std::enable_shared_from_this;
using std::vector;
using std::array;
using std::string;
using std::wstring;
using std::unordered_map;
using std::unordered_set;
using std::map;

// 수학 타입 별칭 (DX11 Engine 의 SimpleMath Vec3/Matrix 대응 → DirectXMath)
using Vec2 = DirectX::XMFLOAT2;
using Vec3 = DirectX::XMFLOAT3;
using Vec4 = DirectX::XMFLOAT4;
using Matrix = DirectX::XMFLOAT4X4;

// 정점 (래스터 입력 + RT BLAS 소스 + GI gather 소스 공용)
struct Vtx
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 nrm;
	DirectX::XMFLOAT3 col;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT3 tan;
};

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
