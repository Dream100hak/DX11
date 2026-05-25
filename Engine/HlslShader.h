#pragma once
#include "ResourceBase.h"
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

// -----------------------------------------------------------
// HlslShader
//  - FX11 ?€???¤ì´?°ë¸Œ DX11 ?°ì´??ë°”ì¸???¸ë°”?¸ë“œ???˜í¼
//  - VS / PS / GS / CS ì§€??
//  - Constant Bufferë¥??¬ë¡¯ ë²ˆí˜¸ë¡?ëª…ì‹œ ë°”ì¸??(b0~b7)
//  - SRV / Sampler ???¬ë¡¯ ì§ì ‘ ?¤ì •
//  - BlendState / RasterizerState / DepthStencilState C++ ê´€ë¦?
// -----------------------------------------------------------

enum class HlslShaderType : uint8
{
	VertexPixel = 0,   // VS + PS
	Compute,           // CS only
};

struct HlslShaderDesc
{
	wstring vsFile;   // e.g. L"Standard_VS.hlsl"
	wstring psFile;   // e.g. L"Standard_PS.hlsl"
	wstring gsFile;   // optional
	wstring hsFile;   // optional (Hull Shader)
	wstring dsFile;   // optional (Domain Shader)
	wstring csFile;   // CS only

	string vsEntry = "VS_Main";
	string psEntry = "PS_Main";
	string gsEntry = "GS_Main";
	string hsEntry = "HS_Main";
	string dsEntry = "DS_Main";
	string csEntry = "CS_Main";
};

class HlslShader : public ResourceBase
{
	using Super = ResourceBase;

public:
	HlslShader();
	virtual ~HlslShader();

	// ?°ì´???ì„± (HlslShaderDesc ?¬ìš©)
	void Create(const HlslShaderDesc& desc);

	// ---- ?Œë” ?íƒœ ?¤ì • ----
	void SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ComPtr<ID3D11RasterizerState> rs);
	void SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef = 0);

	// ---- ?Œì´?„ë¼??ë°”ì¸??----
	void Bind();   // IA ~ OM ?„ì²´ ë°”ì¸??
	void Unbind();// SRV ?´ë¦¬???´ì œ

	// ---- Constant Buffer (ëª…ì‹œ???¬ë¡¯ ë°”ì¸?? ----
	// data ë¥?GPU ë©”ëª¨ë¦¬ì— ?¬ë¦° ???¸ì¶œ - CB??raw ptr
	void SetVSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetPSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetGSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetHSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetDSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetCSConstantBuffer(UINT slot, ID3D11Buffer* buffer);

	// ---- SRV ----
	void SetVSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetPSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetGSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetHSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetDSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetCSSRV(UINT slot, ID3D11ShaderResourceView* srv);

	// ---- UAV (CS ?„ìš©) ----
	void SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav);

	// ---- Sampler ----
	void SetVSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetPSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetHSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetDSSampler(UINT slot, ID3D11SamplerState* sampler);

	// ---- Draw ?¸ì¶œ ----
	void Draw(UINT vertexCount, UINT startVertex = 0);
	void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void DrawTerrainIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void Dispatch(UINT x, UINT y, UINT z);

	// ---- ê³µí†µ Push (ê³µí†µ Shader ?Œë¼ë¯¸í„° ?¤ì •) ----
	void PushGlobalData(const Matrix& view, const Matrix& projection);
	void PushTransformData(const TransformDesc& desc);
	void PushLightData(const LightDesc& desc);
	void PushMaterialData(const MaterialDesc& desc);
	void PushBoneData(const BoneDesc& desc);
	void PushKeyframeData(const KeyframeDesc& desc);
	void PushTweenData(const InstancedTweenDesc& desc);
	void PushLightArrayData(const LightArrayDesc& desc); // ë©€???¼ì´??ë°°ì—´

	// ---- InputLayout ----
	ComPtr<ID3D11InputLayout> GetInputLayout() const { return _inputLayout; }

private:
	// ?°ì´???Œì¼ ì»´íŒŒ????Blob ë°˜í™˜
	ComPtr<ID3DBlob> CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target);
	// InputLayout ?ë™ ?ì„± (VS Blob?ì„œ ë¦¬í”Œ?‰ì…˜)
	void CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob);

private:
	wstring _shaderPath = L"..\\Shaders\\HLSL\\";

	// ?°ì´???¤ë¸Œ?íŠ¸
	ComPtr<ID3D11VertexShader>   _vs;
	ComPtr<ID3D11PixelShader>    _ps;
	ComPtr<ID3D11GeometryShader> _gs;
	ComPtr<ID3D11HullShader>     _hs;
	ComPtr<ID3D11DomainShader>   _ds;
	ComPtr<ID3D11ComputeShader>  _cs;

	ComPtr<ID3D11InputLayout>    _inputLayout;

	// ?Œë” ?íƒœ (nullptr?´ë©´ ê¸°ë³¸ê°’ìœ¼ë¡?ë¦¬ì…‹ ì²˜ë¦¬)
	ComPtr<ID3D11BlendState>         _blendState;
	float           _blendFactor[4] = { 0,0,0,0 };
	UINT     _sampleMask = 0xFFFFFFFF;

	ComPtr<ID3D11RasterizerState>    _rasterizerState;
	ComPtr<ID3D11DepthStencilState>  _depthStencilState;
	UINT            _stencilRef = 0;

	// Constant Buffer (?¬ë¡¯ 0~7 ?´ë? ê´€ë¦?
	shared_ptr<ConstantBuffer<GlobalDesc>>          _globalCB;
	shared_ptr<ConstantBuffer<TransformDesc>>        _transformCB;
	shared_ptr<ConstantBuffer<LightDesc>>         _lightCB;
	shared_ptr<ConstantBuffer<MaterialDesc>>       _materialCB;
	shared_ptr<ConstantBuffer<BoneDesc>>      _boneCB;
	shared_ptr<ConstantBuffer<KeyframeDesc>>         _keyframeCB;
	shared_ptr<ConstantBuffer<InstancedTweenDesc>>   _tweenCB;
	shared_ptr<ConstantBuffer<LightArrayDesc>> _lightArrayCB; // ë©€???¼ì´??CB

	// CB ?¬ë¡¯ ê·œì¹™ (Common.hlsli ?€ ?™ì¼?˜ê²Œ ë§ì¶¤)
	// b0: GlobalBuffer, b1: TransformBuffer, b2: LightBuffer
	// b3: MaterialBuffer, b4: BoneBuffer, b5: KeyframeBuffer, b6: TweenBuffer

	bool _hasCBs = false; // ìµœì´ˆ Push ?¬ë? ?•ì¸
};
