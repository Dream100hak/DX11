#pragma once

// -----------------------------------------------------------
// RenderStateManager
//  - ?뚮뜑留곸뿉 ?꾩슂??BlendState / RasterizerState / DepthStencilState / SamplerState
//    // 瑜?誘몃━ ?앹꽦?대몢怨??대쫫(enum)?쇰줈 議고쉶
//  - Global.fx ???덈뜕 ?ㅼ젙?ㅼ쓣 C++ 濡??닿?
// -----------------------------------------------------------

enum class BlendStateType : uint8
{
	Default = 0,     // ?뚰뙆釉붾젋??鍮꾪솢??
	AlphaBlend,  // SrcAlpha / InvSrcAlpha
	Additive,    // One / One
	AlphaToCoverage, // MSAA ???뚰뙆?대━??
	AdditiveSrcAlpha, // SrcAlpha / One (?뚰떚???섏씠????FX Fire AdditiveBlending ?泥?
	End
};

enum class RasterizerStateType : uint8
{
	SolidCullBack = 0, // 湲곕낯
	SolidCullNone,     // ?묐㈃
	SolidCullFront,    // ?꾩썐?쇱씤 2?⑥뒪
	Wireframe,         // ??댁뼱?꾨젅??
	FrontCounterCW,    // ?ㅼ뭅?대컯????
	ShadowDepth,       // 洹몃┝??留?depth-only (DepthBias ?곸슜, shadow acne 諛⑹?)
	End
};

enum class DepthStencilStateType : uint8
{
	Default = 0,      // Depth R/W ?쒖꽦
	NoDepthWrite,     // Depth Read Only (遺덊닾紐??ㅻ툕?앺듃)
	DisableDepth,     // 源딆씠 鍮꾪솢??(?ъ뒪?명봽濡쒖꽭????ㅽ겕由??⑥뒪)
	OutlineMark,      // ?ㅽ뀗???곌린 (?꾩썐?쇱씤 1?⑥뒪)
	OutlineDraw,      // ?ㅽ뀗???쎄린 (?꾩썐?쇱씤 2?⑥뒪)
	SkyBoxDepth,      // 源딆씠 ?쎄린 ?꾩슜 + LESS_EQUAL (?ㅼ뭅?대컯???꾩슜)
	End
};

enum class SamplerStateType : uint8
{
	Linear = 0,       // MIN_MAG_MIP_LINEAR WRAP
	Point,   // MIN_MAG_MIP_POINT  WRAP
	Anisotropic, // Anisotropic x16
	Shadow,      // ComparisonMinMagLinearMipPoint BORDER (PCF ?꾪꽣留?
	Heightmap,        // MIN_MAG_LINEAR_MIP_POINT CLAMP (吏???믪씠留?
	End
};

class RenderStateManager
{
	DECLARE_SINGLE(RenderStateManager);

public:
	void Init();

	ComPtr<ID3D11BlendState>         GetBS(BlendStateType type)     const;
	ComPtr<ID3D11RasterizerState>    GetRS(RasterizerStateType type)    const;
	ComPtr<ID3D11DepthStencilState>  GetDSS(DepthStencilStateType type) const;
	ComPtr<ID3D11SamplerState>       GetSampler(SamplerStateType type)  const;

	void BindAllSamplersPS() const;
	void BindAllSamplersVS() const;
	void BindAllSamplersGS() const;
	void BindAllSamplersHS() const;
	void BindAllSamplersDS() const;

private:
	void CreateBlendStates();
	void CreateRasterizerStates();
	void CreateDepthStencilStates();
	void CreateSamplerStates();

private:
	static constexpr int BS_COUNT  = static_cast<int>(BlendStateType::End);
	static constexpr int RS_COUNT  = static_cast<int>(RasterizerStateType::End);
	static constexpr int DSS_COUNT = static_cast<int>(DepthStencilStateType::End);
	static constexpr int SS_COUNT  = static_cast<int>(SamplerStateType::End);

	array<ComPtr<ID3D11BlendState>,        BS_COUNT>  _blendStates;
	array<ComPtr<ID3D11RasterizerState>,   RS_COUNT>  _rasterizerStates;
	array<ComPtr<ID3D11DepthStencilState>, DSS_COUNT> _depthStencilStates;
	array<ComPtr<ID3D11SamplerState>,      SS_COUNT>  _samplerStates;
};

#define RENDER_STATES GET_SINGLE(RenderStateManager)
