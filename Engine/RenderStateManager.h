#pragma once

// -----------------------------------------------------------
// RenderStateManager
//  - 렌더링에 필요한 BlendState / RasterizerState / DepthStencilState / SamplerState
//    를 싱글톤으로 생성/관리 (enum으로 검색)
//  - Global.fx 설정값들을 C++ 로 재구현
// -----------------------------------------------------------

enum class BlendStateType : uint8
{
	Default = 0,     // 기본 투명 혼합 비활성
	AlphaBlend,  // SrcAlpha / InvSrcAlpha
	Additive,    // One / One
	AlphaToCoverage, // MSAA 알파 클리핑
	AdditiveSrcAlpha, // SrcAlpha / One (입자 이펙트용, FX Fire AdditiveBlending 호환)
	End
};

enum class RasterizerStateType : uint8
{
	SolidCullBack = 0, // 기본
	SolidCullNone,     // 백페이스
	SolidCullFront,    // 프론트페이스 2패스
	Wireframe,         // 와이어프레임
	FrontCounterCW,    // 시계방향
	ShadowDepth,       // 그림자 맵 depth-only (DepthBias 사용, shadow acne 방지)
	End
};

enum class DepthStencilStateType : uint8
{
	Default = 0,      // Depth R/W 활성
	NoDepthWrite,     // Depth Read Only (불투명 객체 위에 그리기)
	DisableDepth,     // 깊이 비활성 (포스트프로세싱/스카이박스/대체)
	OutlineMark,      // 아웃라인 표시 (프론트페이스 1패스)
	OutlineDraw,      // 아웃라인 그리기 (프론트페이스 2패스)
	SkyBoxDepth,      // 깊이 그리기 미사용 + LESS_EQUAL (스카이박스 사용)
	End
};

enum class SamplerStateType : uint8
{
	Linear = 0,       // MIN_MAG_MIP_LINEAR WRAP
	Point,   // MIN_MAG_MIP_POINT  WRAP
	Anisotropic, // Anisotropic x16
	Shadow,      // ComparisonMinMagLinearMipPoint BORDER (PCF 필터링)
	Heightmap,        // MIN_MAG_LINEAR_MIP_POINT CLAMP (지형 높이맵)
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
