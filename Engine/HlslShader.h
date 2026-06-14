#pragma once
#include "ResourceBase.h"
#include "BindShaderDesc.h"
#include "ConstantBuffer.h"

// -----------------------------------------------------------
// HlslShader
//  - FX11을 제거한 순수 HLSL 셰이더 래퍼 클래스
//  - VS / PS / GS / HS / DS / CS 지원
//  - Constant Buffer 슬롯별 자동 바인딩 (b0~b7)
//  - SRV / Sampler 슬롯별 직접 설정
//  - BlendState / RasterizerState / DepthStencilState C++ 관리
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

	// ---- Stream-Output (입자 이펙트용) ----
	// soEntries가 비어있지 않으면 GS를 CreateGeometryShaderWithStreamOutput으로 생성.
	// FX의 ConstructGSWithSO("POS.xyz; ...") 대체.
	vector<D3D11_SO_DECLARATION_ENTRY> soEntries;
	uint32 soStride = 0;          // SO 버퍼(스트림 아웃) 정점 스트라이드
	bool   soRasterize = false;   // false = SO만 사용 (래스터라이제이션 비활성화)
};

class HlslShader : public ResourceBase
{
	using Super = ResourceBase;

public:
	HlslShader();
	virtual ~HlslShader();

	// 셰이더 생성 (HlslShaderDesc 사용)
	void Create(const HlslShaderDesc& desc);

	// ---- 렌더 상태 설정 ----
	void SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ComPtr<ID3D11RasterizerState> rs);
	void SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef = 0);

	// ---- 셰이더 바인딩 ----
	void Bind();   // IA ~ OM 전체 바인딩
	void Unbind();// SRV 슬롯 해제

	// 전역 와이어프레임 오버라이드 — 켜지면 Bind 가 셰이더 RS 대신 와이어프레임 RS 바인딩.
	// (씬 뷰 와이어프레임 토글: Camera 가 GBuffer 지오메트리 패스 동안만 켠다)
	static bool S_ForceWireframe;

	// ---- Constant Buffer (슬롯별 바인딩) ----
	// 각 스테이지별 CB 설정 - raw ptr로 전달
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

	// ---- UAV (CS 사용) ----
	void SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav);

	// ---- Sampler ----
	void SetVSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetPSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetGSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetHSSampler(UINT slot, ID3D11SamplerState* sampler);
	void SetDSSampler(UINT slot, ID3D11SamplerState* sampler);

	// ---- Draw 호출 ----
	void Draw(UINT vertexCount, UINT startVertex = 0);
	void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawLineIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void DrawTerrainIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);
	void DrawAuto(); // Stream-Output 결과 자동 그리기 (정점 개수 불필요)
	void Dispatch(UINT x, UINT y, UINT z);

	// ---- 공통 Push (공통 셰이더 파라미터 설정) ----
	void PushGlobalData(const Matrix& view, const Matrix& projection);
	void PushTransformData(const TransformDesc& desc);
	void PushLightData(const LightDesc& desc);
	void PushMaterialData(const MaterialDesc& desc);
	void PushBoneData(const BoneDesc& desc);
	void PushModelBoneData(const Matrix& boneTransform); // b5: ModelRenderer 정적 메시 본 변환
	void PushKeyframeData(const KeyframeDesc& desc);
	void PushTweenData(const InstancedTweenDesc& desc);
	void PushLightArrayData(const LightArrayDesc& desc); // 멀티라이팅 배열

	// ---- InputLayout ----
	ComPtr<ID3D11InputLayout> GetInputLayout() const { return _inputLayout; }

private:
	// 셰이더 파일 컴파일하여 Blob 반환
	ComPtr<ID3DBlob> CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target);
	// InputLayout 자동 생성 (VS Blob에서 리플렉션)
	void CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob);

private:
	wstring _shaderPath = L"..\\Shaders\\HLSL\\";

	// 셰이더 객체
	ComPtr<ID3D11VertexShader>   _vs;
	ComPtr<ID3D11PixelShader>    _ps;
	ComPtr<ID3D11GeometryShader> _gs;
	ComPtr<ID3D11HullShader>     _hs;
	ComPtr<ID3D11DomainShader>   _ds;
	ComPtr<ID3D11ComputeShader>  _cs;

	ComPtr<ID3D11InputLayout>    _inputLayout;

	// 렌더 상태 (nullptr이면 기본값으로 렌더링 처리)
	ComPtr<ID3D11BlendState>         _blendState;
	float           _blendFactor[4] = { 0,0,0,0 };
	UINT     _sampleMask = 0xFFFFFFFF;

	ComPtr<ID3D11RasterizerState>    _rasterizerState;
	ComPtr<ID3D11DepthStencilState>  _depthStencilState;
	UINT            _stencilRef = 0;

	// Constant Buffer (슬롯 0~7 자동 관리)
	shared_ptr<ConstantBuffer<GlobalDesc>>          _globalCB;
	shared_ptr<ConstantBuffer<TransformDesc>>        _transformCB;
	shared_ptr<ConstantBuffer<LightDesc>>         _lightCB;
	shared_ptr<ConstantBuffer<MaterialDesc>>       _materialCB;
	shared_ptr<ConstantBuffer<BoneDesc>>      _boneCB;
	shared_ptr<ConstantBuffer<Matrix>>        _modelBoneCB;
	shared_ptr<ConstantBuffer<KeyframeDesc>>         _keyframeCB;
	shared_ptr<ConstantBuffer<InstancedTweenDesc>>   _tweenCB;
	shared_ptr<ConstantBuffer<LightArrayDesc>> _lightArrayCB; // 멀티라이팅 CB

	// CB 슬롯 규칙 (Common.hlsli 매핑과 동일)
	// b0: GlobalBuffer, b1: TransformBuffer, b2: LightBuffer
	// b3: MaterialBuffer, b4: BoneBuffer, b5: KeyframeBuffer, b6: TweenBuffer

	bool _hasCBs = false; // 최초 Push 시점 플래그
};
