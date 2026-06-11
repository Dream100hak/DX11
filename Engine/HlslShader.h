#pragma once
#include "ResourceBase.h"
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

// -----------------------------------------------------------
// HlslShader
//  - FX11 ?占???占쎌씠?占쎈툕 DX11 ?占쎌씠??諛붿씤???占쎈컮?占쎈뱶???占쏀띁
//  - VS / PS / GS / CS 吏??
//  - Constant Buffer占??占쎈’ 踰덊샇占?紐낆떆 諛붿씤??(b0~b7)
//  - SRV / Sampler ???占쎈’ 吏곸젒 ?占쎌젙
//  - BlendState / RasterizerState / DepthStencilState C++ 愿占?
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

	// ---- Stream-Output (?뚰떚???? ----
	// soEntries 媛 鍮꾩뼱?덉? ?딆쑝硫?GS 瑜?CreateGeometryShaderWithStreamOutput ?쇰줈 ?앹꽦.
	// FX ??ConstructGSWithSO("POS.xyz; ...") ?泥?
	vector<D3D11_SO_DECLARATION_ENTRY> soEntries;
	uint32 soStride = 0;          // SO 踰꾪띁(?щ’ 0) ?뺤젏 ?ㅽ듃?쇱씠??
	bool   soRasterize = false;   // false = SO ?꾩슜 (?섏뒪?곕씪?댁쫰 ????
};

class HlslShader : public ResourceBase
{
	using Super = ResourceBase;

public:
	HlslShader();
	virtual ~HlslShader();

	// ?占쎌씠???占쎌꽦 (HlslShaderDesc ?占쎌슜)
	void Create(const HlslShaderDesc& desc);

	// ---- ?占쎈뜑 ?占쏀깭 ?占쎌젙 ----
	void SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ComPtr<ID3D11RasterizerState> rs);
	void SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef = 0);

	// ---- ?占쎌씠?占쎈씪??諛붿씤??----
	void Bind();   // IA ~ OM ?占쎌껜 諛붿씤??
	void Unbind();// SRV ?占쎈━???占쎌젣

	// ---- Constant Buffer (紐낆떆???占쎈’ 諛붿씤?? ----
	// data 占?GPU 硫붾え由ъ뿉 ?占쎈┛ ???占쎌텧 - CB??raw ptr
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

	// ---- UAV (CS ?占쎌슜) ----
	void SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav);

	// ---- Sampler ----
	void SetVSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetPSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetGSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetHSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetDSSampler(UINT slot, ID3D11SamplerState* sampler);

	// ---- Draw ?占쎌텧 ----
	void Draw(UINT vertexCount, UINT startVertex = 0);
	void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawLineIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void DrawTerrainIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawAuto(); // Stream-Output 寃곌낵 ?쒕줈??(?뺤젏 ???먮룞)
	void Dispatch(UINT x, UINT y, UINT z);

	// ---- 怨듯넻 Push (怨듯넻 Shader ?占쎈씪誘명꽣 ?占쎌젙) ----
	void PushGlobalData(const Matrix& view, const Matrix& projection);
	void PushTransformData(const TransformDesc& desc);
	void PushLightData(const LightDesc& desc);
	void PushMaterialData(const MaterialDesc& desc);
	void PushBoneData(const BoneDesc& desc);
	void PushModelBoneData(const Matrix& boneTransform); // b5: ModelRenderer per-mesh 蹂?
	void PushKeyframeData(const KeyframeDesc& desc);
	void PushTweenData(const InstancedTweenDesc& desc);
	void PushLightArrayData(const LightArrayDesc& desc); // 硫???占쎌씠??諛곗뿴

	// ---- InputLayout ----
	ComPtr<ID3D11InputLayout> GetInputLayout() const { return _inputLayout; }

private:
	// ?占쎌씠???占쎌씪 而댄뙆????Blob 諛섑솚
	ComPtr<ID3DBlob> CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target);
	// InputLayout ?占쎈룞 ?占쎌꽦 (VS Blob?占쎌꽌 由ы뵆?占쎌뀡)
	void CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob);

private:
	wstring _shaderPath = L"..\\Shaders\\HLSL\\";

	// ?占쎌씠???占쎈툕?占쏀듃
	ComPtr<ID3D11VertexShader>   _vs;
	ComPtr<ID3D11PixelShader>    _ps;
	ComPtr<ID3D11GeometryShader> _gs;
	ComPtr<ID3D11HullShader>     _hs;
	ComPtr<ID3D11DomainShader>   _ds;
	ComPtr<ID3D11ComputeShader>  _cs;

	ComPtr<ID3D11InputLayout>    _inputLayout;

	// ?占쎈뜑 ?占쏀깭 (nullptr?占쎈㈃ 湲곕낯媛믪쑝占?由ъ뀑 泥섎━)
	ComPtr<ID3D11BlendState>         _blendState;
	float           _blendFactor[4] = { 0,0,0,0 };
	UINT     _sampleMask = 0xFFFFFFFF;

	ComPtr<ID3D11RasterizerState>    _rasterizerState;
	ComPtr<ID3D11DepthStencilState>  _depthStencilState;
	UINT            _stencilRef = 0;

	// Constant Buffer (?占쎈’ 0~7 ?占쏙옙? 愿占?
	shared_ptr<ConstantBuffer<GlobalDesc>>          _globalCB;
	shared_ptr<ConstantBuffer<TransformDesc>>        _transformCB;
	shared_ptr<ConstantBuffer<LightDesc>>         _lightCB;
	shared_ptr<ConstantBuffer<MaterialDesc>>       _materialCB;
	shared_ptr<ConstantBuffer<BoneDesc>>      _boneCB;
	shared_ptr<ConstantBuffer<Matrix>>        _modelBoneCB;
	shared_ptr<ConstantBuffer<KeyframeDesc>>         _keyframeCB;
	shared_ptr<ConstantBuffer<InstancedTweenDesc>>   _tweenCB;
	shared_ptr<ConstantBuffer<LightArrayDesc>> _lightArrayCB; // 硫???占쎌씠??CB

	// CB ?占쎈’ 洹쒖튃 (Common.hlsli ?占??占쎌씪?占쎄쾶 留욎땄)
	// b0: GlobalBuffer, b1: TransformBuffer, b2: LightBuffer
	// b3: MaterialBuffer, b4: BoneBuffer, b5: KeyframeBuffer, b6: TweenBuffer

	bool _hasCBs = false; // 理쒖큹 Push ?占쏙옙? ?占쎌씤
};
