#pragma once

// -----------------------------------------------------------
// RenderStateManager
//  - 렌더링에 필요한 BlendState / RasterizerState / DepthStencilState / SamplerState
//    // 를 미리 생성해두고 이름(enum)으로 조회
//  - Global.fx 에 있던 설정들을 C++ 로 이관
// -----------------------------------------------------------

enum class BlendStateType : uint8
{
	Default = 0,     // 알파블렌드 비활성
	AlphaBlend,  // SrcAlpha / InvSrcAlpha
	Additive,    // One / One
	AlphaToCoverage, // MSAA 용 알파클리핑
	End
};

enum class RasterizerStateType : uint8
{
	SolidCullBack = 0, // 기본
	SolidCullNone,     // 양면
	SolidCullFront,    // 아웃라인 2패스
	Wireframe,         // 와이어프레임
	FrontCounterCW,    // 스카이박스 용
	End
};

enum class DepthStencilStateType : uint8
{
	Default = 0,      // Depth R/W 활성
	NoDepthWrite,     // Depth Read Only (불투명 오브젝트)
	DisableDepth,     // 깊이 비활성 (포스트프로세스 풀스크린 패스)
	OutlineMark,      // 스텐실 쓰기 (아웃라인 1패스)
	OutlineDraw,// 스텐실 읽기 (아웃라인 2패스)
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
