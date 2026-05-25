#include "pch.h"
#include "RenderStateManager.h"

void RenderStateManager::Init()
{
	CreateBlendStates();
	CreateRasterizerStates();
	CreateDepthStencilStates();
	CreateSamplerStates();
}

// ==========================================================================
// Blend States
// ==========================================================================
void RenderStateManager::CreateBlendStates()
{
	// Default : 알파블렌드 비활성
	{
		D3D11_BLEND_DESC desc{};
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;
		desc.RenderTarget[0].BlendEnable = false;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		CHECK(DEVICE->CreateBlendState(&desc, _blendStates[static_cast<int>(BlendStateType::Default)].GetAddressOf()));
	}

	// AlphaBlend : SrcAlpha / InvSrcAlpha
	{
		D3D11_BLEND_DESC desc{};
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = true;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		CHECK(DEVICE->CreateBlendState(&desc, _blendStates[static_cast<int>(BlendStateType::AlphaBlend)].GetAddressOf()));
	}

	// Additive : One / One
	{
		D3D11_BLEND_DESC desc{};
		desc.AlphaToCoverageEnable = true;
		desc.IndependentBlendEnable = false;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = true;
		rt.SrcBlend = D3D11_BLEND_ONE;
		rt.DestBlend = D3D11_BLEND_ONE;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		CHECK(DEVICE->CreateBlendState(&desc, _blendStates[static_cast<int>(BlendStateType::Additive)].GetAddressOf()));
	}

	// AlphaToCoverage
	{
		D3D11_BLEND_DESC desc{};
		desc.AlphaToCoverageEnable = true;
		desc.IndependentBlendEnable = false;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = true;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		CHECK(DEVICE->CreateBlendState(&desc, _blendStates[static_cast<int>(BlendStateType::AlphaToCoverage)].GetAddressOf()));
	}
}

// ==========================================================================
// Rasterizer States
// ==========================================================================
void RenderStateManager::CreateRasterizerStates()
{
	// SolidCullBack (기본)
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = false;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::SolidCullBack)].GetAddressOf()));
	}

	// SolidCullNone
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::SolidCullNone)].GetAddressOf()));
	}

	// SolidCullFront (아웃라인 2패스)
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_FRONT;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::SolidCullFront)].GetAddressOf()));
	}

	// Wireframe
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_WIREFRAME;
		desc.CullMode = D3D11_CULL_BACK;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::Wireframe)].GetAddressOf()));
	}

	// FrontCounterCW (스카이박스)
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = true;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::FrontCounterCW)].GetAddressOf()));
	}
}

// ==========================================================================
// Depth Stencil States
// ==========================================================================
void RenderStateManager::CreateDepthStencilStates()
{
	// Default : Depth R/W 활성
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::Default)].GetAddressOf()));
	}

	// NoDepthWrite : 읽기만 (불투명 오브젝트)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::NoDepthWrite)].GetAddressOf()));
	}

	// DisableDepth : 깊이 비활성 (포스트프로세스 풀스크린 패스)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::DisableDepth)].GetAddressOf()));
	}

	// OutlineMark : 스텐실 쓰기 (아웃라인 1패스)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = true;
		desc.StencilReadMask = 0xFF;
		desc.StencilWriteMask = 0xFF;
		desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.BackFace = desc.FrontFace;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::OutlineMark)].GetAddressOf()));
	}

	// OutlineDraw : 스텐실 읽기 (아웃라인 2패스)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = true;
		desc.StencilReadMask = 0xFF;
		desc.StencilWriteMask = 0x00;
		desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		desc.BackFace = desc.FrontFace;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::OutlineDraw)].GetAddressOf()));
	}

	// SkyBoxDepth : 깊이 읽기 전용 + LESS_EQUAL (스카이박스 전용)
	// xyww 트릭으로 depth=1.0 → 다른 지오메트리가 그린 픽셀(depth<1)은 실패, 빈 배경만 통과
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::SkyBoxDepth)].GetAddressOf()));
	}
}

// ==========================================================================
// Sampler States   (Global.fx 에 있던 샘플러들)
// ==========================================================================
void RenderStateManager::CreateSamplerStates()
{
	// Linear WRAP
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		CHECK(DEVICE->CreateSamplerState(&desc, _samplerStates[static_cast<int>(SamplerStateType::Linear)].GetAddressOf()));
	}

	// Point WRAP
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		CHECK(DEVICE->CreateSamplerState(&desc, _samplerStates[static_cast<int>(SamplerStateType::Point)].GetAddressOf()));
	}

	// Anisotropic x16
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		CHECK(DEVICE->CreateSamplerState(&desc, _samplerStates[static_cast<int>(SamplerStateType::Anisotropic)].GetAddressOf()));
	}

	// Shadow (PCF Comparison)
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0.0f;
		desc.ComparisonFunc = D3D11_COMPARISON_LESS;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		CHECK(DEVICE->CreateSamplerState(&desc, _samplerStates[static_cast<int>(SamplerStateType::Shadow)].GetAddressOf()));
	}

	// Heightmap (지형 높이맵)
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		CHECK(DEVICE->CreateSamplerState(&desc, _samplerStates[static_cast<int>(SamplerStateType::Heightmap)].GetAddressOf()));
	}
}

// ==========================================================================
// Getter
// ==========================================================================
ComPtr<ID3D11BlendState> RenderStateManager::GetBS(BlendStateType type) const
{
	return _blendStates[static_cast<int>(type)];
}
ComPtr<ID3D11RasterizerState> RenderStateManager::GetRS(RasterizerStateType type) const
{
	return _rasterizerStates[static_cast<int>(type)];
}
ComPtr<ID3D11DepthStencilState> RenderStateManager::GetDSS(DepthStencilStateType type) const
{
	return _depthStencilStates[static_cast<int>(type)];
}
ComPtr<ID3D11SamplerState> RenderStateManager::GetSampler(SamplerStateType type) const
{
	return _samplerStates[static_cast<int>(type)];
}

// 전체 Sampler 를 PS 스테이지 s0~s4 에 한번에 바인딩
void RenderStateManager::BindAllSamplersPS() const
{
	ID3D11SamplerState* samplers[SS_COUNT]{};
	for (int i = 0; i < SS_COUNT; i++)
		samplers[i] = _samplerStates[i].Get();
	DCT->PSSetSamplers(0, SS_COUNT, samplers);
}

void RenderStateManager::BindAllSamplersVS() const
{
	ID3D11SamplerState* samplers[SS_COUNT]{};
	for (int i = 0; i < SS_COUNT; i++)
		samplers[i] = _samplerStates[i].Get();
	DCT->VSSetSamplers(0, SS_COUNT, samplers);
}

void RenderStateManager::BindAllSamplersHS() const
{
	ID3D11SamplerState* samplers[SS_COUNT]{};
	for (int i = 0; i < SS_COUNT; i++)
		samplers[i] = _samplerStates[i].Get();
	DCT->HSSetSamplers(0, SS_COUNT, samplers);
}

void RenderStateManager::BindAllSamplersDS() const
{
	ID3D11SamplerState* samplers[SS_COUNT]{};
	for (int i = 0; i < SS_COUNT; i++)
		samplers[i] = _samplerStates[i].Get();
	DCT->DSSetSamplers(0, SS_COUNT, samplers);
}
