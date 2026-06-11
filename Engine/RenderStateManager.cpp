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
	// Default : ?뚰뙆釉붾젋??鍮꾪솢??
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

	// AdditiveSrcAlpha : SrcAlpha / One (FX 01. Fire.fx ??AdditiveBlending ?泥?
	{
		D3D11_BLEND_DESC desc{};
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = true;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_ONE;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ZERO;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		CHECK(DEVICE->CreateBlendState(&desc, _blendStates[static_cast<int>(BlendStateType::AdditiveSrcAlpha)].GetAddressOf()));
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
	// SolidCullBack (湲곕낯)
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

	// SolidCullFront (?꾩썐?쇱씤 2?⑥뒪)
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

	// FrontCounterCW (?ㅼ뭅?대컯??
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = true;
		desc.DepthClipEnable = true;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::FrontCounterCW)].GetAddressOf()));
	}

	// ShadowDepth (洹몃┝??留?depth-only) ??FX 00. ShadowMap.fx ??Depth ?섏뒪?곕씪?댁? ?泥?
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.DepthClipEnable = true;
		// Bias = DepthBias * r + SlopeScaledDepthBias * MaxSlope  (24-bit depth: r = 1/2^24)
		desc.DepthBias = 100000;
		desc.DepthBiasClamp = 0.0f;
		desc.SlopeScaledDepthBias = 1.0f;
		CHECK(DEVICE->CreateRasterizerState(&desc, _rasterizerStates[static_cast<int>(RasterizerStateType::ShadowDepth)].GetAddressOf()));
	}
}

// ==========================================================================
// Depth Stencil States
// ==========================================================================
void RenderStateManager::CreateDepthStencilStates()
{
	// Default : Depth R/W ?쒖꽦
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::Default)].GetAddressOf()));
	}

	// NoDepthWrite : ?쎄린留?(遺덊닾紐??ㅻ툕?앺듃)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::NoDepthWrite)].GetAddressOf()));
	}

	// DisableDepth : 源딆씠 鍮꾪솢??(?ъ뒪?명봽濡쒖꽭????ㅽ겕由??⑥뒪)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = false;
		CHECK(DEVICE->CreateDepthStencilState(&desc, _depthStencilStates[static_cast<int>(DepthStencilStateType::DisableDepth)].GetAddressOf()));
	}

	// OutlineMark : ?ㅽ뀗???곌린 (?꾩썐?쇱씤 1?⑥뒪)
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

	// OutlineDraw : ?ㅽ뀗???쎄린 (?꾩썐?쇱씤 2?⑥뒪)
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

	// SkyBoxDepth : 源딆씠 ?쎄린 ?꾩슜 + LESS_EQUAL (?ㅼ뭅?대컯???꾩슜)
	// xyww ?몃┃?쇰줈 depth=1.0 ???ㅻⅨ 吏?ㅻ찓?몃━媛 洹몃┛ ?쎌?(depth<1)? ?ㅽ뙣, 鍮?諛곌꼍留??듦낵
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
// Sampler States   (Global.fx ???덈뜕 ?섑뵆?щ뱾)
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

	// Heightmap (吏???믪씠留?
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

// ?꾩껜 Sampler 瑜?PS ?ㅽ뀒?댁? s0~s4 ???쒕쾲??諛붿씤??
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

void RenderStateManager::BindAllSamplersGS() const
{
	ID3D11SamplerState* samplers[SS_COUNT]{};
	for (int i = 0; i < SS_COUNT; i++)
		samplers[i] = _samplerStates[i].Get();
	DCT->GSSetSamplers(0, SS_COUNT, samplers);
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
