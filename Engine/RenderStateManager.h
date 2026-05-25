#pragma once

// -----------------------------------------------------------
// RenderStateManager
//  - ���� ���� BlendState / RasterizerState / DepthStencilState / SamplerState
//    �� ���� �����صΰ� �̸�(enum)���� ��ȸ
//  - Global.fx �� �ִ� ���� ���Ǹ� C++ �� ����
// -----------------------------------------------------------

enum class BlendStateType : uint8
{
	Default = 0,     // ������ ��Ȱ��
	AlphaBlend,  // SrcAlpha / InvSrcAlpha
	Additive,    // One / One
	AlphaToCoverage, // MSAA ���� Ŭ���ο�
	End
};

enum class RasterizerStateType : uint8
{
	SolidCullBack = 0, // �⺻
	SolidCullNone,     // ���
	SolidCullFront,    // �ƿ����� 2�н�
	Wireframe,         // ���̾�������
	FrontCounterCW,    // ��ī�̹ڽ� ��
	End
};

enum class DepthStencilStateType : uint8
{
	Default = 0,      // Depth R/W Ȱ��
	NoDepthWrite,     // Depth Read Only (���� ������Ʈ)
	DisableDepth,     // Depth ���� ��Ȱ�� (����Ʈ���μ��� Ǯ��ũ�� ����)
	OutlineMark,      // ���ٽ� ���� (�ƿ����� 1�н�)
	OutlineDraw,// ���ٽ� �б� (�ƿ����� 2�н�)
	End
};

enum class SamplerStateType : uint8
{
	Linear = 0,       // MIN_MAG_MIP_LINEAR WRAP
	Point,   // MIN_MAG_MIP_POINT  WRAP
	Anisotropic, // Anisotropic x16
	Shadow,      // ComparisonMinMagLinearMipPoint BORDER (PCF ������)
	Heightmap,        // MIN_MAG_LINEAR_MIP_POINT CLAMP (���� ���̸�)
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
