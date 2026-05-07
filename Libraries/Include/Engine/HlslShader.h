#pragma once
#include "ResourceBase.h"
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

// -----------------------------------------------------------
// HlslShader
//  - FX11 없이 네이티브 DX11 셰이더를 직접 로드/바인딩하는 래퍼
//  - VS / PS / GS / CS 지원
//  - Constant Buffer는 슬롯 번호로 직접 바인딩 (b0~b7)
//  - SRV / Sampler 도 슬롯 기반
//  - BlendState / RasterizerState / DepthStencilState C++ 제어
// -----------------------------------------------------------

enum class HlslShaderType : uint8
{
	VertexPixel = 0,   // VS + PS
	Compute,           // CS only
};

struct HlslShaderDesc
{
	wstring vsFile;// e.g. L"Standard_VS.hlsl"
	wstring psFile;   // e.g. L"Standard_PS.hlsl"
	wstring gsFile;   // optional
	wstring csFile;   // CS only 모드

	string vsEntry = "VS_Main";
	string psEntry = "PS_Main";
	string gsEntry = "GS_Main";
	string csEntry = "CS_Main";
};

class HlslShader : public ResourceBase
{
	using Super = ResourceBase;

public:
	HlslShader();
	virtual ~HlslShader();

	// 셰이더 로드 (HlslShaderDesc 기반)
	void Create(const HlslShaderDesc& desc);

	// ---- 렌더 상태 설정 ----
	void SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ComPtr<ID3D11RasterizerState> rs);
	void SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef = 0);

	// ---- 파이프라인 바인딩 ----
	void Bind();   // IA ~ OM 전체 바인딩
	void Unbind();// SRV 슬롯 해제

	// ---- Constant Buffer (슬롯 직접 바인딩) ----
	// data 는 GPU 메모리에 이미 업로드된 CB의 raw ptr
	void SetVSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetPSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetGSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
	void SetCSConstantBuffer(UINT slot, ID3D11Buffer* buffer);

	// ---- SRV ----
	void SetVSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetPSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetGSSRV(UINT slot, ID3D11ShaderResourceView* srv);
	void SetCSSRV(UINT slot, ID3D11ShaderResourceView* srv);

	// ---- UAV (CS 전용) ----
	void SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav);

	// ---- Sampler ----
	void SetVSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetPSSampler(UINT slot, ID3D11SamplerState* sampler);

	// ---- Draw 호출 ----
	void Draw(UINT vertexCount, UINT startVertex = 0);
	void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void Dispatch(UINT x, UINT y, UINT z);

	// ---- 편의 Push (기존 Shader 인터페이스 유지) ----
	void PushGlobalData(const Matrix& view, const Matrix& projection);
	void PushTransformData(const TransformDesc& desc);
	void PushLightData(const LightDesc& desc);
	void PushMaterialData(const MaterialDesc& desc);
	void PushBoneData(const BoneDesc& desc);
	void PushKeyframeData(const KeyframeDesc& desc);
	void PushTweenData(const InstancedTweenDesc& desc);

	// ---- InputLayout ----
	ComPtr<ID3D11InputLayout> GetInputLayout() const { return _inputLayout; }

private:
	// 셰이더 파일 컴파일 → Blob
	ComPtr<ID3DBlob> CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target);
	// InputLayout 자동 반영 (VS Blob의 시그니처 리플렉션)
	void CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob);

private:
	wstring _shaderPath = L"..\\Shaders\\HLSL\\";

	// 셰이더 오브젝트
	ComPtr<ID3D11VertexShader>   _vs;
	ComPtr<ID3D11PixelShader>    _ps;
	ComPtr<ID3D11GeometryShader> _gs;
	ComPtr<ID3D11ComputeShader>  _cs;

	ComPtr<ID3D11InputLayout>    _inputLayout;

	// 렌더 상태 (nullptr이면 파이프라인 디폴트 사용)
	ComPtr<ID3D11BlendState>         _blendState;
	float           _blendFactor[4] = { 0,0,0,0 };
	UINT     _sampleMask = 0xFFFFFFFF;

	ComPtr<ID3D11RasterizerState>    _rasterizerState;
	ComPtr<ID3D11DepthStencilState>  _depthStencilState;
	UINT            _stencilRef = 0;

	// Constant Buffer (슬롯 0~7 사전 생성)
	shared_ptr<ConstantBuffer<GlobalDesc>>          _globalCB;
	shared_ptr<ConstantBuffer<TransformDesc>>        _transformCB;
	shared_ptr<ConstantBuffer<LightDesc>>         _lightCB;
	shared_ptr<ConstantBuffer<MaterialDesc>>       _materialCB;
	shared_ptr<ConstantBuffer<BoneDesc>>      _boneCB;
	shared_ptr<ConstantBuffer<KeyframeDesc>>         _keyframeCB;
	shared_ptr<ConstantBuffer<InstancedTweenDesc>>   _tweenCB;

	// CB 슬롯 약속 (Common.hlsli 와 동일하게 맞춤)
	// b0: GlobalBuffer, b1: TransformBuffer, b2: LightBuffer
	// b3: MaterialBuffer, b4: BoneBuffer, b5: KeyframeBuffer, b6: TweenBuffer

	bool _hasCBs = false; // 최초 Push 시 생성
};
