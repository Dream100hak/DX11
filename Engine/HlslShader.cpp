#include "pch.h"
#include "HlslShader.h"
#include "Light.h"
#include "Utils.h"

HlslShader::HlslShader() : Super(ResourceType::Shader)
{
}

HlslShader::~HlslShader()
{
}

// --------------------------------------------------------------------------
// Create : HlslShaderDesc를 받아 각 셰이더를 생성하는 함수
// --------------------------------------------------------------------------
void HlslShader::Create(const HlslShaderDesc& desc)
{
	// Compute Shader 전용
	if (!desc.csFile.empty())
	{
		auto csBlob = CompileShaderFromFile(_shaderPath + desc.csFile, desc.csEntry, "cs_5_0");
		HRESULT hr = DEVICE->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, _cs.GetAddressOf());
		CHECK(hr);
		return;
	}

	// VS (필수)
	if (!desc.vsFile.empty())
	{
		auto vsBlob = CompileShaderFromFile(_shaderPath + desc.vsFile, desc.vsEntry, "vs_5_0");
		HRESULT hr = DEVICE->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, _vs.GetAddressOf());
		CHECK(hr);
		CreateInputLayoutFromVS(vsBlob);
	}

	// PS (선택)
	if (!desc.psFile.empty())
	{
		auto psBlob = CompileShaderFromFile(_shaderPath + desc.psFile, desc.psEntry, "ps_5_0");
		HRESULT hr = DEVICE->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, _ps.GetAddressOf());
		CHECK(hr);
	}

	// GS (선택)
	if (!desc.gsFile.empty())
	{
		auto gsBlob = CompileShaderFromFile(_shaderPath + desc.gsFile, desc.gsEntry, "gs_5_0");
		HRESULT hr = DEVICE->CreateGeometryShader(gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(), nullptr, _gs.GetAddressOf());
		CHECK(hr);
	}

	// HS (선택)
	if (!desc.hsFile.empty())
	{
		auto hsBlob = CompileShaderFromFile(_shaderPath + desc.hsFile, desc.hsEntry, "hs_5_0");
		HRESULT hr = DEVICE->CreateHullShader(hsBlob->GetBufferPointer(), hsBlob->GetBufferSize(), nullptr, _hs.GetAddressOf());
		CHECK(hr);
	}

	// DS (선택)
	if (!desc.dsFile.empty())
	{
		auto dsBlob = CompileShaderFromFile(_shaderPath + desc.dsFile, desc.dsEntry, "ds_5_0");
		HRESULT hr = DEVICE->CreateDomainShader(dsBlob->GetBufferPointer(), dsBlob->GetBufferSize(), nullptr, _ds.GetAddressOf());
		CHECK(hr);
	}
}

// --------------------------------------------------------------------------
// 렌더 상태 설정
// --------------------------------------------------------------------------
void HlslShader::SetBlendState(ComPtr<ID3D11BlendState> bs, const float blendFactor[4], UINT sampleMask)
{
	_blendState = bs;
	if (blendFactor)
		memcpy(_blendFactor, blendFactor, sizeof(float) * 4);
	_sampleMask = sampleMask;
}

void HlslShader::SetRasterizerState(ComPtr<ID3D11RasterizerState> rs)
{
	_rasterizerState = rs;
}

void HlslShader::SetDepthStencilState(ComPtr<ID3D11DepthStencilState> dss, UINT stencilRef)
{
	_depthStencilState = dss;
	_stencilRef = stencilRef;
}

// --------------------------------------------------------------------------
// Bind : 파이프라인에 셰이더를 바인드
// --------------------------------------------------------------------------
void HlslShader::Bind()
{
	auto dct = DCT;

	if (_inputLayout) dct->IASetInputLayout(_inputLayout.Get());
	if (_vs)          dct->VSSetShader(_vs.Get(), nullptr, 0);
	if (_ps)          dct->PSSetShader(_ps.Get(), nullptr, 0);
	else              dct->PSSetShader(nullptr, nullptr, 0);
	if (_gs)          dct->GSSetShader(_gs.Get(), nullptr, 0);
	else              dct->GSSetShader(nullptr, nullptr, 0);
	if (_hs)          dct->HSSetShader(_hs.Get(), nullptr, 0);
	else              dct->HSSetShader(nullptr, nullptr, 0);
	if (_ds)          dct->DSSetShader(_ds.Get(), nullptr, 0);
	else              dct->DSSetShader(nullptr, nullptr, 0);
	if (_cs)          dct->CSSetShader(_cs.Get(), nullptr, 0);

	if (_blendState)         dct->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
	else                     dct->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	if (_rasterizerState)    dct->RSSetState(_rasterizerState.Get());
	else                     dct->RSSetState(nullptr);

	if (_depthStencilState)  dct->OMSetDepthStencilState(_depthStencilState.Get(), _stencilRef);
	else                     dct->OMSetDepthStencilState(nullptr, 0);
}

void HlslShader::Unbind()
{
	ID3D11ShaderResourceView* nullSRV[8] = {};
	DCT->VSSetShaderResources(0, 8, nullSRV);
	DCT->PSSetShaderResources(0, 8, nullSRV);
	if (_gs) DCT->GSSetShaderResources(0, 8, nullSRV);
	if (_hs) { DCT->HSSetShader(nullptr, nullptr, 0); }
	if (_ds) { DCT->DSSetShaderResources(0, 8, nullSRV); DCT->DSSetShader(nullptr, nullptr, 0); }
}

// --------------------------------------------------------------------------
// Constant Buffer 명시적 바인드
// --------------------------------------------------------------------------
void HlslShader::SetVSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->VSSetConstantBuffers(slot, 1, &buffer);
}
void HlslShader::SetPSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->PSSetConstantBuffers(slot, 1, &buffer);
}
void HlslShader::SetGSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->GSSetConstantBuffers(slot, 1, &buffer);
}
void HlslShader::SetHSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->HSSetConstantBuffers(slot, 1, &buffer);
}
void HlslShader::SetDSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->DSSetConstantBuffers(slot, 1, &buffer);
}
void HlslShader::SetCSConstantBuffer(UINT slot, ID3D11Buffer* buffer)
{
	DCT->CSSetConstantBuffers(slot, 1, &buffer);
}

// --------------------------------------------------------------------------
// SRV
// --------------------------------------------------------------------------
void HlslShader::SetVSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->VSSetShaderResources(slot, 1, &srv);
}
void HlslShader::SetPSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->PSSetShaderResources(slot, 1, &srv);
}
void HlslShader::SetGSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->GSSetShaderResources(slot, 1, &srv);
}
void HlslShader::SetHSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->HSSetShaderResources(slot, 1, &srv);
}
void HlslShader::SetDSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->DSSetShaderResources(slot, 1, &srv);
}
void HlslShader::SetCSSRV(UINT slot, ID3D11ShaderResourceView* srv)
{
	DCT->CSSetShaderResources(slot, 1, &srv);
}

// --------------------------------------------------------------------------
// UAV (CS 전용)
// --------------------------------------------------------------------------
void HlslShader::SetCSUAV(UINT slot, ID3D11UnorderedAccessView* uav)
{
	DCT->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
}

// --------------------------------------------------------------------------
// Sampler
// --------------------------------------------------------------------------
void HlslShader::SetVSSampler(UINT slot, ID3D11SamplerState* sampler)
{
	DCT->VSSetSamplers(slot, 1, &sampler);
}
void HlslShader::SetPSSampler(UINT slot, ID3D11SamplerState* sampler)
{
	DCT->PSSetSamplers(slot, 1, &sampler);
}
void HlslShader::SetHSSampler(UINT slot, ID3D11SamplerState* sampler)
{
	DCT->HSSetSamplers(slot, 1, &sampler);
}
void HlslShader::SetDSSampler(UINT slot, ID3D11SamplerState* sampler)
{
	DCT->DSSetSamplers(slot, 1, &sampler);
}

// --------------------------------------------------------------------------
// Draw / Dispatch
// --------------------------------------------------------------------------
void HlslShader::Draw(UINT vertexCount, UINT startVertex)
{
	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Bind();
	DCT->Draw(vertexCount, startVertex);
}

void HlslShader::DrawIndexed(UINT indexCount, UINT startIndex, INT baseVertex)
{
	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Bind();
	DCT->DrawIndexed(indexCount, startIndex, baseVertex);
}

void HlslShader::DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex, UINT startInstance)
{
	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Bind();
	DCT->DrawInstanced(vertexCountPerInstance, instanceCount, startVertex, startInstance);
}

void HlslShader::DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndex, INT baseVertex, UINT startInstance)
{
	DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Bind();
	DCT->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
}

void HlslShader::DrawTerrainIndexed(UINT indexCount, UINT startIndex, INT baseVertex)
{
	DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
	Bind();
	DCT->DrawIndexed(indexCount, startIndex, baseVertex);
}

void HlslShader::Dispatch(UINT x, UINT y, UINT z)
{
	if (_cs) DCT->CSSetShader(_cs.Get(), nullptr, 0);
	DCT->Dispatch(x, y, z);
}

// --------------------------------------------------------------------------
// Push 함수들 (공통 Shader 파라미터 설정, b0~b6, VS+PS 동시 바인드)
// --------------------------------------------------------------------------
void HlslShader::PushGlobalData(const Matrix& view, const Matrix& projection)
{
	if (!_globalCB)
	{
		_globalCB = make_shared<ConstantBuffer<GlobalDesc>>();
		_globalCB->Create();
	}

	Matrix T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	GlobalDesc desc;
	desc.V = view;
	desc.P = projection;
	desc.VP = view * projection;
	desc.VInv = view.Invert();
	desc.Shadow = Light::S_Shadow;
	desc.T = T;

	_globalCB->CopyData(desc);
	auto buf = _globalCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(0, 1, &buf);
	DCT->PSSetConstantBuffers(0, 1, &buf);
	if (_gs) DCT->GSSetConstantBuffers(0, 1, &buf);
	if (_hs) DCT->HSSetConstantBuffers(0, 1, &buf);
	if (_ds) DCT->DSSetConstantBuffers(0, 1, &buf);
}

void HlslShader::PushTransformData(const TransformDesc& desc)
{
	if (!_transformCB)
	{
		_transformCB = make_shared<ConstantBuffer<TransformDesc>>();
		_transformCB->Create();
	}
	_transformCB->CopyData(const_cast<TransformDesc&>(desc));
	auto buf = _transformCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(1, 1, &buf);
	DCT->PSSetConstantBuffers(1, 1, &buf);
}

void HlslShader::PushLightData(const LightDesc& desc)
{
	if (!_lightCB)
	{
		_lightCB = make_shared<ConstantBuffer<LightDesc>>();
		_lightCB->Create();
	}
	_lightCB->CopyData(const_cast<LightDesc&>(desc));
	auto buf = _lightCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(2, 1, &buf);
	DCT->PSSetConstantBuffers(2, 1, &buf);
}

void HlslShader::PushMaterialData(const MaterialDesc& desc)
{
	if (!_materialCB)
	{
		_materialCB = make_shared<ConstantBuffer<MaterialDesc>>();
		_materialCB->Create();
	}
	_materialCB->CopyData(const_cast<MaterialDesc&>(desc));
	auto buf = _materialCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(3, 1, &buf);
	DCT->PSSetConstantBuffers(3, 1, &buf);
}

void HlslShader::PushBoneData(const BoneDesc& desc)
{
	if (!_boneCB)
	{
		_boneCB = make_shared<ConstantBuffer<BoneDesc>>();
		_boneCB->Create();
	}
	_boneCB->CopyData(const_cast<BoneDesc&>(desc));
	auto buf = _boneCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(4, 1, &buf);
}

void HlslShader::PushKeyframeData(const KeyframeDesc& desc)
{
	if (!_keyframeCB)
	{
		_keyframeCB = make_shared<ConstantBuffer<KeyframeDesc>>();
		_keyframeCB->Create();
	}
	_keyframeCB->CopyData(const_cast<KeyframeDesc&>(desc));
	auto buf = _keyframeCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(5, 1, &buf);
}

void HlslShader::PushTweenData(const InstancedTweenDesc& desc)
{
	if (!_tweenCB)
	{
		_tweenCB = make_shared<ConstantBuffer<InstancedTweenDesc>>();
		_tweenCB->Create();
	}
	_tweenCB->CopyData(const_cast<InstancedTweenDesc&>(desc));
	auto buf = _tweenCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(6, 1, &buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// PushLightArrayData - 멀티 라이트 배열 전송
// ─────────────────────────────────────────────────────────────────────────────
void HlslShader::PushLightArrayData(const LightArrayDesc& desc)
{
	if (!_lightArrayCB)
	{
		_lightArrayCB = make_shared<ConstantBuffer<LightArrayDesc>>();
		_lightArrayCB->Create();
	}
	_lightArrayCB->CopyData(const_cast<LightArrayDesc&>(desc));
	auto buf = _lightArrayCB->GetComPtr().Get();
	DCT->VSSetConstantBuffers(7, 1, &buf);
	DCT->PSSetConstantBuffers(7, 1, &buf);
}

// --------------------------------------------------------------------------
// Internal : HLSL 셰이더 컴파일
// --------------------------------------------------------------------------
ComPtr<ID3DBlob> HlslShader::CompileShaderFromFile(const wstring& filePath, const string& entryPoint, const string& target)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	UINT compileFlags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	HRESULT hr = D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.c_str(),
		target.c_str(),
		compileFlags,
		0,
		shaderBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			string errMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
			MessageBoxA(nullptr, errMsg.c_str(), "HlslShader Compile Error", MB_OK | MB_ICONERROR);
		}
		assert(false && "HlslShader: shader compile failed");
	}

	return shaderBlob;
}

// --------------------------------------------------------------------------
// Internal : VS Blob 리플렉션으로 InputLayout 자동 생성
// --------------------------------------------------------------------------
void HlslShader::CreateInputLayoutFromVS(ComPtr<ID3DBlob> vsBlob)
{
	if (!vsBlob)
	{
		assert(false && "HlslShader: vsBlob is null");
		return;
	}

	ComPtr<ID3D11ShaderReflection> reflection;
	HRESULT hr = D3DReflect(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection, reinterpret_cast<void**>(reflection.GetAddressOf()));
	
	if (FAILED(hr) || !reflection)
	{
		assert(false && "HlslShader: D3DReflect failed or reflection is null");
		return;
	}

	D3D11_SHADER_DESC shaderDesc;
	reflection->GetDesc(&shaderDesc);

	vector<D3D11_INPUT_ELEMENT_DESC> inputLayout;
	UINT byteOffset_Slot0 = 0;
	UINT byteOffset_Slot1 = 0;

	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
		reflection->GetInputParameterDesc(i, &paramDesc);

		// SV_ 시스템 시맨틱 InputLayout에서 제외
		string semantic = paramDesc.SemanticName;
		if (semantic.rfind("SV_", 0) == 0)
			continue;

		D3D11_INPUT_ELEMENT_DESC elem{};
		elem.SemanticName     = paramDesc.SemanticName;
		elem.SemanticIndex     = paramDesc.SemanticIndex;
		elem.AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT;
		elem.InputSlotClass  = D3D11_INPUT_PER_VERTEX_DATA;
		elem.InstanceDataStepRate = 0;
		elem.InputSlot = 0;

		// 포맷 추론 (기본)
		if      (paramDesc.Mask == 1)   elem.Format = (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) ? DXGI_FORMAT_R32_FLOAT    : DXGI_FORMAT_R32_UINT;
		else if (paramDesc.Mask <= 3)   elem.Format = (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) ? DXGI_FORMAT_R32G32_FLOAT      : DXGI_FORMAT_R32G32_UINT;
		else if (paramDesc.Mask <= 7)   elem.Format = (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) ? DXGI_FORMAT_R32G32B32_FLOAT   : DXGI_FORMAT_R32G32B32_UINT;
		else elem.Format = (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R32G32B32A32_UINT;

		string name = semantic;
		transform(name.begin(), name.end(), name.begin(), ::toupper);

		// POSITION은 항상 R32G32B32_FLOAT
		if (name == "POSITION")
			elem.Format = DXGI_FORMAT_R32G32B32_FLOAT;

		// INST* (Instancing) 또는 PICKED는 InputSlot 1에 할당
		if (name.rfind("INST", 0) == 0 || name == "PICKED")
		{
			elem.InputSlot = 1;
			elem.InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
			elem.InstanceDataStepRate = 1;
			elem.AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT;
			if (name == "PICKED")
				elem.Format = DXGI_FORMAT_R32_UINT;
		}

		// INST_WORLD (matrix): 4개의 float4로 분리 - 리플렉션의 index 0~3을 위해
		// index 0에서만 4개 추가하고, 나머지 index(1~3)는 스킵
		if (name == "INST_WORLD")
		{
			if (paramDesc.SemanticIndex == 0)
			{
				for (int j = 0; j < 4; j++)
				{
					D3D11_INPUT_ELEMENT_DESC matElem{};
					matElem.SemanticName    = "INST_WORLD";
					matElem.SemanticIndex         = j;
					matElem.Format      = DXGI_FORMAT_R32G32B32A32_FLOAT;
					matElem.InputSlot          = 1;
					matElem.InputSlotClass        = D3D11_INPUT_PER_INSTANCE_DATA;
					matElem.InstanceDataStepRate  = 1;
					matElem.AlignedByteOffset     = D3D11_APPEND_ALIGNED_ELEMENT;
					inputLayout.push_back(matElem);
				}
			}
			continue;  // index 0~3 모두 스킵 (위에서 이미 4개 추가함)
		}

		inputLayout.push_back(elem);
	}

	// InputLayout이 있는 경우에만 생성
	if (!inputLayout.empty())
	{
		hr = DEVICE->CreateInputLayout(
			inputLayout.data(),
			static_cast<UINT>(inputLayout.size()),
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			_inputLayout.GetAddressOf()
		);

		if (FAILED(hr))
		{
			// 오류 로그
			assert(false && "HlslShader: CreateInputLayout failed");
			return;
		}
	}
}
