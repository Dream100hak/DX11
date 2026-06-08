#pragma once
#include "ResourceBase.h"
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

// -----------------------------------------------------------
// HlslShader
//  - FX11 ?�???�이?�브 DX11 ?�이??바인???�바?�드???�퍼
//  - VS / PS / GS / CS 지??
//  - Constant Buffer�??�롯 번호�?명시 바인??(b0~b7)
//  - SRV / Sampler ???�롯 직접 ?�정
//  - BlendState / RasterizerState / DepthStencilState C++ 관�?
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

	// ?�이???�성 (HlslShaderDesc ?�용)
	void Create(const HlslShaderDesc& desc);

	// ---- ?�더 ?�태 ?�정 ----
	void SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ComPtr<ID3D11RasterizerState> rs);
	void SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef = 0);

	// ---- ?�이?�라??바인??----
	void Bind();   // IA ~ OM ?�체 바인??
	void Unbind();// SRV ?�리???�제

	// ---- Constant Buffer (명시???�롯 바인?? ----
	// data �?GPU 메모리에 ?�린 ???�출 - CB??raw ptr
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

	// ---- UAV (CS ?�용) ----
	void SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav);

	// ---- Sampler ----
	void SetVSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetPSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetHSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetDSSampler(UINT slot, ID3D11SamplerState* sampler);

	// ---- Draw ?�출 ----
	void Draw(UINT vertexCount, UINT startVertex = 0);
	void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawLineIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void DrawTerrainIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void Dispatch(UINT x, UINT y, UINT z);

	// ---- 공통 Push (공통 Shader ?�라미터 ?�정) ----
	void PushGlobalData(const Matrix& view, const Matrix& projection);
	void PushTransformData(const TransformDesc& desc);
	void PushLightData(const LightDesc& desc);
	void PushMaterialData(const MaterialDesc& desc);
	void PushBoneData(const BoneDesc& desc);
	void PushKeyframeData(const KeyframeDesc& desc);
	void PushTweenData(const InstancedTweenDesc& desc);
	void PushLightArrayData(const LightArrayDesc& desc); // 멀???�이??배열

	// ---- InputLayout ----
	ComPtr<ID3D11InputLayout> GetInputLayout() const { return _inputLayout; }

private:
	// ?�이???�일 컴파????Blob 반환
	ComPtr<ID3DBlob> CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target);
	// InputLayout ?�동 ?�성 (VS Blob?�서 리플?�션)
	void CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob);

private:
	wstring _shaderPath = L"..\\Shaders\\HLSL\\";

	// ?�이???�브?�트
	ComPtr<ID3D11VertexShader>   _vs;
	ComPtr<ID3D11PixelShader>    _ps;
	ComPtr<ID3D11GeometryShader> _gs;
	ComPtr<ID3D11HullShader>     _hs;
	ComPtr<ID3D11DomainShader>   _ds;
	ComPtr<ID3D11ComputeShader>  _cs;

	ComPtr<ID3D11InputLayout>    _inputLayout;

	// ?�더 ?�태 (nullptr?�면 기본값으�?리셋 처리)
	ComPtr<ID3D11BlendState>         _blendState;
	float           _blendFactor[4] = { 0,0,0,0 };
	UINT     _sampleMask = 0xFFFFFFFF;

	ComPtr<ID3D11RasterizerState>    _rasterizerState;
	ComPtr<ID3D11DepthStencilState>  _depthStencilState;
	UINT            _stencilRef = 0;

	// Constant Buffer (?�롯 0~7 ?��? 관�?
	shared_ptr<ConstantBuffer<GlobalDesc>>          _globalCB;
	shared_ptr<ConstantBuffer<TransformDesc>>        _transformCB;
	shared_ptr<ConstantBuffer<LightDesc>>         _lightCB;
	shared_ptr<ConstantBuffer<MaterialDesc>>       _materialCB;
	shared_ptr<ConstantBuffer<BoneDesc>>      _boneCB;
	shared_ptr<ConstantBuffer<KeyframeDesc>>         _keyframeCB;
	shared_ptr<ConstantBuffer<InstancedTweenDesc>>   _tweenCB;
	shared_ptr<ConstantBuffer<LightArrayDesc>> _lightArrayCB; // 멀???�이??CB

	// CB ?�롯 규칙 (Common.hlsli ?� ?�일?�게 맞춤)
	// b0: GlobalBuffer, b1: TransformBuffer, b2: LightBuffer
	// b3: MaterialBuffer, b4: BoneBuffer, b5: KeyframeBuffer, b6: TweenBuffer

	bool _hasCBs = false; // 최초 Push ?��? ?�인
};
