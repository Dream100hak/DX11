#include "pch.h"
#include "ShaderBuffer.h"
#include "Utils.h"

ShaderBuffer::ShaderBuffer(wstring file) : _file(file)
{

	auto shader = RESOURCES->Get<Shader>(Utils::GetResourcesName(file, L".fx"));

	if(shader == nullptr)
		_shader = make_shared<Shader>(_file);
	else
		_shader = shader;

	Init();
}

ShaderBuffer::ShaderBuffer(shared_ptr<Shader> shader) : _shader(shader)
{
	Init();
}

ShaderBuffer::~ShaderBuffer()
{

}

void ShaderBuffer::Init()
{
	_initialStateBlock = make_shared<StateBlock>();
	{
		DC->RSGetState(_initialStateBlock->RSRasterizerState.GetAddressOf());
		DC->OMGetBlendState(_initialStateBlock->OMBlendState.GetAddressOf(), _initialStateBlock->OMBlendFactor, &_initialStateBlock->OMSampleMask);
		DC->OMGetDepthStencilState(_initialStateBlock->OMDepthStencilState.GetAddressOf(), &_initialStateBlock->OMStencilRef);
	}

	CreateEffect();
}

void ShaderBuffer::CreateEffect()
{

	_shader->GetEffect()->GetDesc(&_effectDesc);
	for (UINT t = 0; t < _effectDesc.Techniques; t++)
	{
		Technique technique;
		technique.technique = _shader->GetEffect()->GetTechniqueByIndex(t);
		technique.technique->GetDesc(&technique.desc);
		technique.name = Utils::ToWString(technique.desc.Name);

		for (UINT p = 0; p < technique.desc.Passes; p++)
		{
			Pass pass;
			pass.pass = technique.technique->GetPassByIndex(p);
			pass.pass->GetDesc(&pass.desc);
			pass.name = Utils::ToWString(pass.desc.Name);
			pass.pass->GetVertexShaderDesc(&pass.passVsDesc);
			pass.passVsDesc.pShaderVariable->GetShaderDesc(pass.passVsDesc.ShaderIndex, &pass.effectVsDesc);

			for (UINT s = 0; s < pass.effectVsDesc.NumInputSignatureEntries; s++)
			{
				D3D11_SIGNATURE_PARAMETER_DESC desc;

				HRESULT hr = pass.passVsDesc.pShaderVariable->GetInputSignatureElementDesc(pass.passVsDesc.ShaderIndex, s, &desc);
				CHECK(hr);

				pass.signatureDescs.push_back(desc);
			}

			pass.inputLayout = CreateInputLayout(_shader->GetBlob(), &pass.effectVsDesc, pass.signatureDescs);
			pass.stateBlock = _initialStateBlock;

			technique.passes.push_back(pass);
		}

		_techniques.push_back(technique);
	}

	for (UINT i = 0; i < _effectDesc.ConstantBuffers; i++)
	{
		ID3DX11EffectConstantBuffer* iBuffer;
		iBuffer = _shader->GetEffect()->GetConstantBufferByIndex(i);

		D3DX11_EFFECT_VARIABLE_DESC vDesc;
		iBuffer->GetDesc(&vDesc);
	}

	for (UINT i = 0; i < _effectDesc.GlobalVariables; i++)
	{
		ID3DX11EffectVariable* effectVariable;
		effectVariable = _shader->GetEffect()->GetVariableByIndex(i);

		D3DX11_EFFECT_VARIABLE_DESC vDesc;
		effectVariable->GetDesc(&vDesc);
	}
}

ComPtr<ID3D11InputLayout> ShaderBuffer::CreateInputLayout(ComPtr<ID3DBlob> fxBlob, D3DX11_EFFECT_SHADER_DESC* effectVsDesc, vector<D3D11_SIGNATURE_PARAMETER_DESC>& params)
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;

	for (D3D11_SIGNATURE_PARAMETER_DESC& paramDesc : params)
	{
		D3D11_INPUT_ELEMENT_DESC elementDesc;
		elementDesc.SemanticName = paramDesc.SemanticName;
		elementDesc.SemanticIndex = paramDesc.SemanticIndex;
		elementDesc.InputSlot = 0;
		elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;

		if (paramDesc.Mask == 1)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
		}
		else if (paramDesc.Mask <= 3)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (paramDesc.Mask <= 7)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (paramDesc.Mask <= 15)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}

		string name = paramDesc.SemanticName;
		std::transform(name.begin(), name.end(), name.begin(), toupper);

		if (name == "POSITION")
		{
			elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}

		if (Utils::StartsWith(name, "INST") == true)
		{
			elementDesc.InputSlot = 1;
			elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			elementDesc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
			elementDesc.InstanceDataStepRate = 1;
		}

		if (Utils::StartsWith(name, "SV_") == false)
			inputLayoutDesc.push_back(elementDesc);
	}

	const void* code = effectVsDesc->pBytecode;
	UINT codeSize = effectVsDesc->BytecodeLength;

	if (inputLayoutDesc.size() > 0)
	{
		ComPtr<ID3D11InputLayout> inputLayout;

		HRESULT hr = DEVICE->CreateInputLayout
		(
			&inputLayoutDesc[0]
			, inputLayoutDesc.size()
			, code
			, codeSize
			, inputLayout.GetAddressOf()
		);

		CHECK(hr);

		return inputLayout;
	}

	return nullptr;
}

void ShaderBuffer::Draw(UINT technique, UINT pass, UINT vertexCount, UINT startVertexLocation)
{
	_techniques[technique].passes[pass].Draw(vertexCount, startVertexLocation);
}

void ShaderBuffer::DrawIndexed(UINT technique, UINT pass, UINT indexCount, UINT startIndexLocation, INT baseVertexLocation)
{
	_techniques[technique].passes[pass].DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
}

void ShaderBuffer::DrawInstanced(UINT technique, UINT pass, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
{
	_techniques[technique].passes[pass].DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void ShaderBuffer::DrawIndexedInstanced(UINT technique, UINT pass, UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation)
{
	_techniques[technique].passes[pass].DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void ShaderBuffer::Dispatch(UINT technique, UINT pass, UINT x, UINT y, UINT z)
{
	_techniques[technique].passes[pass].Dispatch(x, y, z);
}

ComPtr<ID3DX11EffectVariable> ShaderBuffer::GetVariable(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str());
}

ComPtr<ID3DX11EffectScalarVariable> ShaderBuffer::GetScalar(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsScalar();
}

ComPtr<ID3DX11EffectVectorVariable> ShaderBuffer::GetVector(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsVector();
}

ComPtr<ID3DX11EffectMatrixVariable> ShaderBuffer::GetMatrix(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsMatrix();
}

ComPtr<ID3DX11EffectStringVariable> ShaderBuffer::GetString(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsString();
}

ComPtr<ID3DX11EffectShaderResourceVariable> ShaderBuffer::GetSRV(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsShaderResource();
}

ComPtr<ID3DX11EffectRenderTargetViewVariable> ShaderBuffer::GetRTV(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsRenderTargetView();
}

ComPtr<ID3DX11EffectDepthStencilViewVariable> ShaderBuffer::GetDSV(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsDepthStencilView();
}

ComPtr<ID3DX11EffectConstantBuffer> ShaderBuffer::GetConstantBuffer(string name)
{
	return _shader->GetEffect()->GetConstantBufferByName(name.c_str());
}

ComPtr<ID3DX11EffectShaderVariable> ShaderBuffer::GetShader(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsShader();
}

ComPtr<ID3DX11EffectBlendVariable> ShaderBuffer::GetBlend(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsBlend();
}

ComPtr<ID3DX11EffectDepthStencilVariable> ShaderBuffer::GetDepthStencil(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsDepthStencil();
}

ComPtr<ID3DX11EffectRasterizerVariable> ShaderBuffer::GetRasterizer(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsRasterizer();
}

ComPtr<ID3DX11EffectSamplerVariable> ShaderBuffer::GetSampler(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsSampler();
}

ComPtr<ID3DX11EffectUnorderedAccessViewVariable> ShaderBuffer::GetUAV(string name)
{
	return _shader->GetEffect()->GetVariableByName(name.c_str())->AsUnorderedAccessView();
}

void ShaderBuffer::PushGlobalData(const Matrix& view, const Matrix& projection)
{
	if (_globalEffectBuffer == nullptr)
	{
		_globalBuffer = make_shared<ConstantBuffer<GlobalDesc>>();
		_globalBuffer->Create();
		_globalEffectBuffer = GetConstantBuffer("GlobalBuffer");
	}

	_globalDesc.V = view;
	_globalDesc.P = projection;
	_globalDesc.VP = view * projection;
	_globalDesc.VInv = view.Invert();
	_globalBuffer->CopyData(_globalDesc);
	_globalEffectBuffer->SetConstantBuffer(_globalBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushTransformData(const TransformDesc& desc)
{
	if (_transformEffectBuffer == nullptr)
	{
		_transformBuffer = make_shared<ConstantBuffer<TransformDesc>>();
		_transformBuffer->Create();
		_transformEffectBuffer = GetConstantBuffer("TransformBuffer");
	}

	_transformDesc = desc;
	_transformBuffer->CopyData(_transformDesc);
	_transformEffectBuffer->SetConstantBuffer(_transformBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushLightData(const LightDesc& desc)
{
	if (_lightEffectBuffer == nullptr)
	{
		_lightBuffer = make_shared<ConstantBuffer<LightDesc>>();
		_lightBuffer->Create();
		_lightEffectBuffer = GetConstantBuffer("LightBuffer");
	}

	_lightDesc = desc;
	_lightBuffer->CopyData(_lightDesc);
	_lightEffectBuffer->SetConstantBuffer(_lightBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushMaterialData(const MaterialDesc& desc)
{
	if (_materialEffectBuffer == nullptr)
	{
		_materialBuffer = make_shared<ConstantBuffer<MaterialDesc>>();
		_materialBuffer->Create();
		_materialEffectBuffer = GetConstantBuffer("MaterialBuffer");
	}

	_materialDesc = desc;
	_materialBuffer->CopyData(_materialDesc);
	_materialEffectBuffer->SetConstantBuffer(_materialBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushBoneData(const BoneDesc& desc)
{
	if (_boneEffectBuffer == nullptr)
	{
		_boneBuffer = make_shared<ConstantBuffer<BoneDesc>>();
		_boneBuffer->Create();
		_boneEffectBuffer = GetConstantBuffer("BoneBuffer");
	}

	_boneDesc = desc;
	_boneBuffer->CopyData(_boneDesc);
	_boneEffectBuffer->SetConstantBuffer(_boneBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushKeyframeData(const KeyframeDesc& desc)
{
	if (_keyframeEffectBuffer == nullptr)
	{
		_keyframeBuffer = make_shared<ConstantBuffer<KeyframeDesc>>();
		_keyframeBuffer->Create();
		_keyframeEffectBuffer = GetConstantBuffer("KeyframeBuffer");
	}

	_keyframeDesc = desc;
	_keyframeBuffer->CopyData(_keyframeDesc);
	_keyframeEffectBuffer->SetConstantBuffer(_keyframeBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushTweenData(const InstancedTweenDesc& desc)
{
	if (_transformEffectBuffer == nullptr)
	{
		_tweenBuffer = make_shared<ConstantBuffer<InstancedTweenDesc>>();
		_tweenBuffer->Create();
		_tweenEffectBuffer = GetConstantBuffer("TweenBuffer");
	}

	_tweenDesc = desc;
	_tweenBuffer->CopyData(_tweenDesc);
	_tweenEffectBuffer->SetConstantBuffer(_tweenBuffer->GetComPtr().Get());
}

void ShaderBuffer::PushSnowData(const SnowBillboardDesc& desc)
{
	if (_snowEffectBuffer == nullptr)
	{
		_snowBuffer = make_shared<ConstantBuffer<SnowBillboardDesc>>();
		_snowBuffer->Create();
		_snowEffectBuffer = GetConstantBuffer("SnowBuffer");
	}

	_snowDesc = desc;
	_snowBuffer->CopyData(_snowDesc);
	_snowEffectBuffer->SetConstantBuffer(_snowBuffer->GetComPtr().Get());
}