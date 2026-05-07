#pragma once

// -----------------------------------------------------------
// RenderStateManager
//  - 자주 쓰는 BlendState / RasterizerState / DepthStencilState / SamplerState
//    를 사전 생성해두고 이름(enum)으로 조회
//  - Global.fx 에 있던 상태 정의를 C++ 로 이전
// -----------------------------------------------------------

enum class BlendStateType : uint8
{
	Default = 0,     // 블렌딩 비활성
	AlphaBlend,  // SrcAlpha / InvSrcAlpha
	Additive,    // One / One
	AlphaToCoverage, // MSAA 알파 클리핑용
	End
};

enum class RasterizerStateType : uint8
{
	SolidCullBack = 0, // 기본
	SolidCullNone,     // 양면
	SolidCullFront,    // 아웃라인 2패스
	Wireframe,         // 와이어프레임
	FrontCounterCW,    // 스카이박스 등
	End
};

enum class DepthStencilStateType : uint8
{
	Default = 0,      // Depth R/W 활성
	NoDepthWrite,     // Depth Read Only (투명 오브젝트)
	DisableDepth,     // Depth 완전 비활성 (포스트프로세스 풀스크린 쿼드)
	OutlineMark,      // 스텐실 쓰기 (아웃라인 1패스)
	OutlineDraw,// 스텐실 읽기 (아웃라인 2패스)
	End
};

enum class SamplerStateType : uint8
{
	Linear = 0,       // MIN_MAG_MIP_LINEAR WRAP
	Point,   // MIN_MAG_MIP_POINT  WRAP
	Anisotropic, // Anisotropic x16
	Shadow,      // ComparisonMinMagLinearMipPoint BORDER (PCF 섀도우)
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

	// 전체 Sampler 배열을 PS 슬롯 0~N 에 한번에 바인딩
	void BindAllSamplersPS() const;
	void BindAllSamplersVS() const;

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
